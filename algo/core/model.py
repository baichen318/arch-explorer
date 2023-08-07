# Author: baichen.bai@alibaba-inc.com


import networkx as nx
from enum import Enum
from copy import deepcopy
from typing import List, Tuple
from collections import OrderedDict
from utils.thread import WorkerThread
from algo.core.visualize import Visualization
from utils.exceptions import UnSupportedException
from algo.core.instruction import RiscvInstruction
from utils.utils import info, error, warn, assert_error, \
    Timer
from algo.core.arch_bottleneck import bottleneck as BTNK
from algo.core.arch_bottleneck import IcacheMiss, Base, \
    DcacheMiss, BPMiss, ROB, LQ, SQ, IntRF, FpRF, \
    IQ, IntAlu, IntMultDiv, FpAlu, FpMultDiv, RdWrPort, \
    RAW, Virtual


class PipelineStage(Enum):
    # fetch cache line
    F1 = "F1"
    # receive from icache
    F2 = "F2"
    # fetch
    fetch = "F"
    # merge F2 & fetch
    F2F = "F2/F"
    # decode
    decode = "D"
    # rename
    rename = "R"
    # dispatch
    dispatch = "DS"
    # issue
    issue = "I"
    # merge dispatch & issue
    DI = "D/I"
    # memory
    memory = "M"
    # complete
    complete = "P"
    # merge memory & complete
    MP = "M/P"
    # commit
    commit = "C"


class PipelineDelay(Enum):
    icache_hit_delay = 2
    dcache_hit_delay = 2


class Stats(object):
    def __init__(self):
        super(Stats, self).__init__()
        self.br_count = 0
        self.br_squash_count = 0
        self.icache_access_count = 0
        self.icache_miss_count = 0
        self.dcache_access_count = 0
        self.dcache_miss_count = 0
        self.bottleneck = self.contruct_bottleneck()

    def contruct_bottleneck(self):
        bottleneck = OrderedDict()
        for btnk in BTNK:
            bottleneck[btnk] = 0
        return bottleneck

    def update_br_count(self):
        self.br_count += 1

    def update_br_squash_count(self):
        self.br_squash_count += 1

    def update_icache_access_count(self):
        self.icache_access_count += 1

    def update_icache_miss_count(self):
        self.icache_miss_count += 1

    def update_dcache_access_count(self):
        self.dcache_access_count += 1

    def dcache_miss_count(self):
        self.dcache_miss_count += 1


class InstMeta(object):
    """
        Extend with meta information.
        `InstMeta` is designed to accelerate
        the meta information access.
    """
    def __init__(self, inst: RiscvInstruction):
        super(InstMeta, self).__init__()
        self.inst = inst
        # `stages` saves each pipeline stage
        self.stages = []
        self.dependent_stages = []

    @property
    def first_dependent_stage(self):
        if len(self.dependent_stages) > 0:
            return self.dependent_stages[0]

    def add_stage(self, stage):
        """
            stage: Graph.Node
        """
        self.stages.append(stage)

    def add_stages(self, stages):
        """
            stages: List[Graph.Node]
        """
        for stage in stages:
            self.add_stage(stage)
        # sort from small to large according to the timestamp
        self.stages.sort(key=lambda node: node.timestamp)

    def if_exist_stage(self, dependent_stage):
        """
            dependent_stage: Graph.Node
        """
        for _dependent_stage in self.dependent_stages:
            if dependent_stage.seq == _dependent_stage.seq and \
                dependent_stage.timestamp == _dependent_stage.timestamp:
                return True
        return False

    def add_dependent_stages(self, dependent_stages):
        """
            dependent_stages: List[Graph.Node]
            Duplications may exist in `dependent_stages`.
        """
        for dependent_stage in dependent_stages:
            if not self.if_exist_stage(dependent_stage):
                self.dependent_stages.append(dependent_stage)
        if len(self.dependent_stages) > 0:
            # sort from small to large according to the timestamp
            self.dependent_stages.sort(key=lambda node: node.timestamp)


class InstsMetaBuffer(object):
    """
        Store meta information for each instruction.
    """
    def __init__(self):
        super(InstsMetaBuffer, self).__init__()
        self.meta = []

    def __len__(self):
        return len(self.meta)

    @property
    def is_empty(self):
        return len(self) == 0

    def __getitem__(self, item: int):
        try:
            return self.meta[item]
        except IndexError:
            raise IndexError(
                "idx: {} of \"meta\" is " \
                "out of bounds.".format(idx)
            )

    def __iter__(self):
        for meta in self.meta:
            yield meta

    def first(self):
        return self.meta[-1]

    def last(self):
        return self.meta[0]

    def insert(self, inst_meta: InstMeta):
        self.meta.insert(0, inst_meta)


class InstsBuffer(object):
    """
        Store historical instructions.
    """
    def __init__(self):
        super(InstsBuffer, self).__init__()
        self.insts = []

    def __len__(self):
        return len(self.insts)

    @property
    def is_empty(self):
        return len(self) == 0

    def __getitem__(self, item: int):
        try:
            return self.insts[item]
        except IndexError:
            raise IndexError(
                "idx: {} of \"insts\" is " \
                "out of bounds.".format(idx)
            )

    def __iter__(self):
        for inst in self.insts:
            yield inst

    def first(self):
        return self.insts[-1]

    def last(self):
        return self.insts[0]

    def insert(self, inst: RiscvInstruction):
        self.insts.insert(0, inst)


class Graph(Stats):

    class Node(object):
        def __init__(
            self, stage: Enum,
            coordinate: Tuple[int, int],
            inst: RiscvInstruction
        ):
            super(Graph.Node, self).__init__()
            self._stage = stage
            # (timestamp, sequence number)
            self.coordinate = coordinate
            self.inst = inst
            # (sequence number)-(pipeline stage)
            self.name = "{}-{}".format(
                self.seq, self.stage
            )
            assert self.seq == self.coordinate[1]

        @property
        def stage(self):
            return self._stage.value

        @stage.setter
        def stage(self, stage):
            self._stage = stage

        @property
        def timestamp(self):
            return self.coordinate[0]

        @property
        def seq(self):
            return self.coordinate[1]

        def __eq__(self, node):
            return self.seq == node.seq and \
                self.timestamp == node.timestamp

        def __repr__(self):
            msg = self.name
            if self.stage == "F1":
                return "{}\n{}".format(
                    msg,
                    self.inst.inst
                )
            return msg

    class Edge(object):
        def __init__(
            self,
            u,
            v,
            delay: int,
            cost: int,
            bottleneck,
            critical: bool = False
        ):
            super(Graph.Edge, self).__init__()
            # the start vertex
            self.u = u
            # the end vertex
            self.v = v
            # delayed cycles
            self.delay = delay
            # `cost` is used in the critical path
            # construction
            self.cost = cost
            # `bottleneck` illustrates the root cause
            self.bottleneck = bottleneck
            # `critical` is True if the edge in on
            # the critical path
            self.critical = critical
            # `virtual` is used in the critical
            # path construction
            self.virtual = False

        def set_critical(self):
            self.critical = True

        def mask_virtual(self):
            self.virtual = True

        def __repr__(self):
            return "{}\n{}".format(
                self.bottleneck,
                self.delay
            )

    def __init__(self):
        super(Graph, self).__init__()
        self.graph = nx.DiGraph(attri="new-deg")
        self.insts = InstsBuffer()
        # self.meta = InstsMetaBuffer()
        self.last_access_icache = 0

    @property
    def nodes(self):
        return self.graph.nodes

    @property
    def edges(self):
        return self.graph.edges

    @property
    def __len__(self):
        return len(self.nodes)

    @property
    def source_node(self):
        return self.get_node_via_inst(
            self.insts.first(), PipelineStage.F1
        )

    @property
    def sink_node(self):
        return self.get_node_via_inst(
            self.insts.last(), PipelineStage.commit
        )

    @property
    def num_of_inst(self):
        return self.sink_node.seq

    def succ(self, item):
        return self.graph.succ[item]

    def pred(self, item):
        return self.graph.pred[item]

    def out_edges(self, item):
        return self.graph.out_edges(item)

    def in_edges(self, item):
        return self.graph.in_edges(item)

    def get_node_via_inst(self, inst: RiscvInstruction, stage: Enum):
        """
            Since we merge some nodes, we need to check all
            potential combinations.
            F2, F, F2/F
            D, I, D/I
            M, P, M/P
        """
        name = self.unique_name_via_inst(inst, stage)
        try:
            return self.nodes[name]["node"]
        except KeyError:
            try:
                if stage == PipelineStage.fetch or \
                    stage == PipelineStage.F2:
                    name = self.unique_name_via_inst(
                        inst, PipelineStage.F2F
                    )
                elif stage == PipelineStage.issue or \
                    stage == PipelineStage.dispatch:
                    name = self.unique_name_via_inst(
                        inst, PipelineStage.DI
                    )
                elif stage == PipelineStage.memory or \
                    stage == PipelineStage.complete:
                    name = self.unique_name_via_inst(
                        inst, PipelineStage.MP
                    )
                return self.nodes[name]["node"]
            except KeyError:
                raise KeyError(
                    "name: {} is not found.".format(name)
                )

    def get_node_via_name(self, name: str):
        try:
            return self.nodes[name]["node"]
        except KeyError:
            raise KeyError(
                "name: {} is not found.".format(name)
            )

    def get_edge(self, u: str, v: str):
        try:
            return self.edges[(u, v)]["edge"]
        except KeyError:
            raise KeyError(
                "edge: {} -> {} is not found.".format(
                    u, v
                )
            )

    def unique_name_via_inst(
        self, inst: RiscvInstruction, stage: Enum
    ):
        return "{}-{}".format(
            inst.seq, stage.value
        )

    def get_seq_via_name(self, name: str):
        return int(name.split('-')[0])

    def add_node(self, node: Node):
        """
            We distinguish between different
            instructions' nodes.
        """

        self.graph.add_node(
            node.name, node=node
        )

    def add_nodes(self, nodes: List[Node]):
        for node in nodes:
            self.add_node(node)

    def add_edge(self, edge: Edge):
        self.graph.add_edge(
            edge.u.name,
            edge.v.name,
            edge=edge,
            cost=edge.cost
        )

    def add_edges(self, edges: List[Edge]):
        for edge in edges:
            self.add_edge(edge)

    def if_exist_edge(self, u: Node, v: Node) -> bool:
        return (u.name, v.name) in self.edges

    def get_inst_via_idx(self, idx: int):
        return self.insts[idx]

    def get_inst_via_seq(self, seq: int):
        return self.get_inst_via_idx(len(self.insts) - seq)

    # def get_meta_via_idx(self, idx: int):
    #     """
    #         DEPRECATED.
    #     """
    #     return self.meta[idx]

    # def get_meta_via_seq(self, seq: int):
    #     """
    #         DEPRECATED.
    #     """
    #     return self.get_meta_via_idx(len(self.meta) - seq)

    def view(self, path, start: int, end: int, format="pdf"):
        """
            Visualize the DAG (DEBUG usage).
        """
        visualization = Visualization(self)
        try:
            dot = visualization.draw(
                self.graph, start, end
            )
            dot.format = format
            info("generating graph: {}".format(path))
            dot.render(path, cleanup=True)
        except UnSupportedException as e:
            error(e)

    def model(self, inst: RiscvInstruction):
        # `F1` vertex
        nodes = []
        F1 = self.Node(PipelineStage.F1,
            (inst.fetch_cache_line, inst.seq),
            inst
        )
        nodes.append(F1)

        # verify if `inst` has `F2`, `fetch`, and `F2/F` vertex
        if inst.process_cache_completion == inst.fetch:
            F2F = self.Node(
                PipelineStage.F2F,
                (inst.fetch, inst.seq),
                inst
            )
            nodes.append(F2F)
        else:
            F2 = self.Node(
                PipelineStage.F2,
                (inst.process_cache_completion, inst.seq),
                inst
            )
            nodes.append(F2)
            fetch = self.Node(
                PipelineStage.fetch,
                (inst.fetch, inst.seq),
                inst
            )
            nodes.append(fetch)

        # `decode` vertex
        decode = self.Node(
            PipelineStage.decode,
            (inst.decode, inst.seq),
            inst
        )
        nodes.append(decode)
        # `rename` vertex
        rename = self.Node(
            PipelineStage.rename,
            (inst.rename, inst.seq),
            inst
        )
        nodes.append(rename)

        # verify if `inst` has `dispatch`, `issue` or `D/I`
        if inst.dispatch == inst.issue:
            DI = self.Node(
                PipelineStage.DI,
                (inst.dispatch, inst.seq),
                inst
            )
            nodes.append(DI)
        else:
            dispatch = self.Node(
                PipelineStage.dispatch,
                (inst.dispatch, inst.seq),
                inst
            )
            nodes.append(dispatch)
            issue = self.Node(
                PipelineStage.issue,
                (inst.issue, inst.seq),
                inst
            )
            nodes.append(issue)

        """
            If `inst` is a store, since `complete` = `memory`,
            `inst` has only `M/P`. If `inst` is a load,
            `complete` = `complete-memory`
        """
        if inst.is_mem:
            if inst.is_store:
                MP = self.Node(
                    PipelineStage.MP,
                    (inst.memory, inst.seq),
                    inst
                )
                nodes.append(MP)
            if inst.is_load:
                # verify if `inst` has `memory`, `complete`
                # or `M/P`
                if inst.memory == inst.complete_memory:
                    MP = self.Node(
                        PipelineStage.MP,
                        (inst.memory, inst.seq),
                        inst
                    )
                    nodes.append(MP)
                else:
                    memory = self.Node(
                        PipelineStage.memory,
                        (inst.memory, inst.seq),
                        inst
                    )
                    nodes.append(memory)
                    complete = self.Node(
                        PipelineStage.complete,
                        (inst.complete_memory, inst.seq),
                        inst
                    )
                    nodes.append(complete)
        else:
            complete = self.Node(
                PipelineStage.complete,
                (inst.complete, inst.seq),
                inst
            )
            nodes.append(complete)

        # `commit` vertex
        commit = self.Node(
            PipelineStage.commit,
            (inst.commit, inst.seq), inst
        )
        nodes.append(commit)

        # add nodes
        self.add_nodes(nodes)

        edges = []

        """
            F1 -> F2 -> fetch -> decode or F1 -> F2/F -> decode
        """
        if inst.process_cache_completion == inst.fetch:
            # F1 -> F2/F -> decode
            delay = inst.process_cache_completion - \
                inst.fetch_cache_line
            edges.append(
                self.Edge(
                    F1, F2F,
                    delay, 0,
                    Base() \
                        if delay == PipelineDelay.icache_hit_delay.value \
                            else IcacheMiss()
                )
            )
            delay = inst.decode - inst.fetch
            edges.append(
                self.Edge(
                    F2F, decode,
                    delay, 0,
                    Base()
                )
            )
        else:
            # F1 -> F2 -> fetch -> decode
            delay = inst.process_cache_completion - \
                inst.fetch_cache_line
            edges.append(
                self.Edge(
                    F1, F2,
                    delay, 0,
                    Base() if delay == PipelineDelay.icache_hit_delay.value \
                        else IcacheMiss()
                )
            )
            delay = inst.fetch - inst.process_cache_completion
            edges.append(
                self.Edge(
                    F2, fetch,
                    delay, 0,
                    Base()
                )
            )
            edges.append(
                self.Edge(
                    fetch, decode,
                    inst.decode - inst.fetch, 0,
                    Base()
                )
            )

        # connect decode -> rename
        edges.append(
            self.Edge(
                decode, rename,
                inst.rename - inst.decode, 0,
                Base()
            )
        )

        # verify rename -> dispatch -> issue or rename -> D/I
        if inst.dispatch == inst.issue:
            # rename -> D/I
            edges.append(
                self.Edge(
                    rename, DI,
                    inst.issue - inst.rename, 0,
                    Base()
                )
            )
        else:
            # rename -> dispatch -> issue
            edges.append(
                self.Edge(
                    rename, dispatch,
                    inst.dispatch - inst.rename, 0,
                    Base()
                )
            )
            edges.append(
                self.Edge(
                    dispatch, issue,
                    inst.issue - inst.dispatch, 0,
                    Base()
                )
            )

        # verify dispatch -> issue -> complete (normal),
        # dispatch -> issue -> M/P (store/load),
        # dispatch -> issue -> memory -> complete (load),
        # D/I -> complete (normal),
        # D/I -> M/P (store/load),
        # D/I -> memory -> complete (load)
        if inst.is_mem:
            if inst.is_store:
                if inst.dispatch == inst.issue:
                    # D/I -> M/P
                    edges.append(self.Edge(
                            DI, MP,
                            inst.memory - inst.dispatch, 0,
                            Base()
                        )
                    )
                else:
                    # dispatch -> issue -> M/P
                    edges.append(self.Edge(
                            dispatch, issue,
                            inst.issue - inst.dispatch, 0,
                            Base()
                        )
                    )
                    edges.append(self.Edge(
                            issue, MP,
                            inst.complete - inst.issue, 0,
                            Base()
                        )
                    )
            else:
                if inst.dispatch == inst.issue:
                    if inst.memory == inst.complete_memory:
                        # D/I -> M/P
                        delay = inst.complete - inst.dispatch
                        edges.append(self.Edge(
                                DI, MP,
                                delay, 0,
                                Base() \
                                    if delay == \
                                        PipelineDelay.dcache_hit_delay.value \
                                        else DcacheMiss()
                            )
                        )
                    else:
                        # D/I -> memory -> complete
                        edges.append(self.Edge(
                                DI, memory,
                                inst.memory - inst.issue, 0,
                                Base()
                            )
                        )
                        delay = inst.complete_memory - inst.memory
                        edges.append(self.Edge(
                                memory, complete,
                                inst.complete_memory - inst.memory, 0,
                                Base() \
                                    if delay == \
                                        PipelineDelay.dcache_hit_delay.value \
                                        else DcacheMiss()
                            )
                        )
                else:
                    if inst.memory == inst.complete_memory:
                        # dispatch -> issue -> M/P
                        delay = inst.issue - inst.dispatch
                        edges.append(self.Edge(
                                dispatch, issue,
                                delay, 0,
                                Base()
                            )
                        )
                        edges.append(self.Edge(
                                issue, MP,
                                inst.complete_memory - inst.issue, 0,
                                Base()
                            )
                        )
                    else:
                        # dispatch -> issue -> memory -> complete
                        delay = inst.issue - inst.dispatch
                        edges.append(self.Edge(
                                dispatch, issue,
                                delay, 0,
                                Base()
                            )
                        )
                        edges.append(self.Edge(
                                issue, memory,
                                inst.memory - inst.issue, 0,
                                Base()
                            )
                        )
                        delay = inst.complete_memory - inst.memory
                        edges.append(self.Edge(
                                memory, complete,
                                delay, 0,
                                Base() \
                                    if delay == \
                                        PipelineDelay.dcache_hit_delay.value \
                                        else DcacheMiss()
                            )
                        )
        else:
            if inst.dispatch == inst.issue:
                # D/I -> complete
                edges.append(self.Edge(
                        DI, complete,
                        inst.complete - inst.issue, 0,
                        Base()
                    )
                )
            else:
                # dispatch -> issue -> complete
                edges.append(self.Edge(
                        dispatch, issue,
                        inst.issue - inst.dispatch, 0,
                        Base()
                    )
                )
                edges.append(self.Edge(
                        issue, complete,
                        inst.complete - inst.issue, 0,
                        Base()
                    )
                )

        # verify complete -> commit (normal/load),
        # M/P -> commit (store/load)
        if inst.is_store:
            edges.append(self.Edge(
                    MP, commit,
                    inst.commit - inst.complete, 0,
                    Base()
                )
            )
        elif inst.is_load:
            if inst.memory == inst.complete_memory:
                # M/P -> commit
                edges.append(self.Edge(
                        MP, commit,
                        inst.commit - inst.complete, 0,
                        Base()
                    )
                )
            else:
                # complete -> commit
                edges.append(self.Edge(
                        complete, commit,
                        inst.commit - inst.complete_memory, 0,
                        Base()
                    )
                )
        else:
            # complete -> commit
            edges.append(self.Edge(
                    complete, commit,
                    inst.commit - inst.complete, 0,
                    Base()
                )
            )

        # add edges
        self.add_edges(edges)

        if inst.is_control:
            self.update_br_count()
        elif inst.is_mem:
            self.update_dcache_access_count()

        if inst.fetch_cache_line > self.last_access_icache:
            self.last_access_icache = inst.fetch_cache_line
            self.update_icache_access_count()

        """
            DEPRECATED.
        """
        # # add meta information
        # meta = InstMeta(inst)
        # meta.add_stages(nodes)
        # self.meta.insert(meta)

    def model_miss_bp_interaction(self, inst: RiscvInstruction):
        prev = self.insts.last()
        if prev.is_control:
            if prev.complete < inst.fetch_cache_line:
                # miss branch prediction
                delay = inst.fetch_cache_line - prev.complete
                self.add_edge(self.Edge(
                        self.get_node_via_inst(
                            prev, PipelineStage.complete
                        ),
                        self.get_node_via_inst(
                            inst, PipelineStage.F1
                        ),
                        delay, delay,
                        BPMiss()
                    )
                )
                return self.get_node_via_inst(
                        inst, PipelineStage.F1
                    )

    def model_rob_interaction(self, inst: RiscvInstruction):
        if inst.block_from_rob != 0:
            # a ROB dependence
            for _inst in self.insts:
                if _inst.rob == inst.rob:
                    delay = inst.rename - _inst.rename
                    self.add_edge(self.Edge(
                            self.get_node_via_inst(
                                _inst,
                                PipelineStage.rename
                            ),
                            self.get_node_via_inst(
                                inst,
                                PipelineStage.rename
                            ),
                            delay, delay,
                            ROB()
                        )
                    )
                    return self.get_node_via_inst(
                            inst, PipelineStage.rename
                        )

    def model_lq_interaction(self, inst: RiscvInstruction):
        if inst.block_from_lq != 0 and inst.is_load:
            for _inst in self.insts:
                if _inst.lq == inst.lq:
                    delay = inst.rename - _inst.rename
                    self.add_edge(self.Edge(
                            self.get_node_via_inst(
                                _inst,
                                PipelineStage.rename
                            ),
                            self.get_node_via_inst(
                                inst,
                                PipelineStage.rename
                            ),
                            delay, delay,
                            LQ()
                        )
                    )
                    return self.get_node_via_inst(
                            inst, PipelineStage.rename
                        )

    def model_sq_interaction(self, inst: RiscvInstruction):
        if inst.block_from_sq != 0 and inst.is_store:
            for _inst in self.insts:
                if _inst.sq == inst.sq:
                    delay = inst.rename - _inst.rename
                    self.add_edge(self.Edge(
                            self.get_node_via_inst(
                                _inst,
                                PipelineStage.rename
                            ),
                            self.get_node_via_inst(
                                inst,
                                PipelineStage.rename
                            ),
                            delay, delay,
                            SQ()
                        )
                    )
                    return self.get_node_via_inst(
                            inst, PipelineStage.rename
                        )

    def raw_dep(
        self, tgt: RiscvInstruction, src: RiscvInstruction
    ):
        assert tgt.seq > src.seq
        return src.dst in tgt.src

    def war_dep(
        self, tgt: RiscvInstruction, src: RiscvInstruction
    ):
        assert tgt.seq > src.seq
        return tgt.dst in src.src

    def wrw_dep(
        self, tgt: RiscvInstruction, src: RiscvInstruction
    ):
        return tgt.dst == src.dst

    def model_rf_interaction(self, inst: RiscvInstruction):
        if inst.block_from_rf != 0:
            t = 0
            dependency = None
            for _inst in self.insts:
                if self.war_dep(inst, _inst) or \
                    self.wrw_dep(inst, _inst):
                    dependency = _inst
                    break
            if dependency is not None:
                """
                    The `dependency` could be None since
                    the trace could be imcomplete.
                """
                prev_node = self.get_node_via_inst(
                    dependency, PipelineStage.rename
                )
                node = self.get_node_via_inst(
                    inst, PipelineStage.rename
                )
                if inst.use_int_rf:
                    bottleneck = IntRF()
                elif inst.use_fp_rf:
                    bottleneck = FpRF()
                else:
                    # No_OpClass could reach here
                    return
                delay = inst.rename - dependency.rename
                self.add_edge(self.Edge(
                        prev_node, node,
                        delay, delay,
                        bottleneck
                    )
                )
                return node

    def model_iq_interaction(self, inst: RiscvInstruction):
        if inst.block_from_iq != 0:
            for _inst in self.insts:
                if inst.iq == _inst.iq:
                    delay = inst.rename - _inst.rename
                    self.add_edge(self.Edge(
                            self.get_node_via_inst(
                                _inst, PipelineStage.rename
                            ),
                            self.get_node_via_inst(
                                inst, PipelineStage.rename
                            ),
                            delay, delay,
                            IQ()
                        )
                    )
                    return self.get_node_via_inst(
                            inst, PipelineStage.rename
                        )

    def model_fu_interaction(self, inst: RiscvInstruction):
        if inst.issue > inst.insert_ready_list:
            for _inst in self.insts:
                # TODO: `_inst` should be complete before `inst`?
                if inst.fu == _inst.fu and \
                    _inst.issue < inst.issue:
                    fu_dependence = False
                    if inst.is_load and \
                        _inst.complete_memory >= inst.issue:
                        fu_dependence = True
                    else:
                        if _inst.complete >= inst.issue:
                            fu_dependence = True
                    if fu_dependence:
                        prev_node = self.get_node_via_inst(
                            _inst, PipelineStage.issue
                        )
                        node = self.get_node_via_inst(
                            inst, PipelineStage.issue
                        )
                        if inst.use_int_alu:
                            bottleneck = IntAlu()
                        elif inst.use_int_mult_div:
                            bottleneck = IntMultDiv()
                        elif inst.use_fp_alu:
                            bottleneck = FpAlu()
                        elif inst.use_fp_mult_div:
                            bottleneck = FpMultDiv()
                        elif inst.use_rd_wr_port:
                            bottleneck = RdWrPort()
                        else:
                            # No_OpClass could reach here
                            continue
                        delay = inst.issue - _inst.issue
                        self.add_edge(self.Edge(
                                prev_node, node,
                                delay, delay,
                                bottleneck
                            )
                        )
                        return node

    def model_raw_interaction(self, inst: RiscvInstruction, CONST=50):
        """
            We set `CONST` here to accelerate the dependency
            search process. We make an assumption that at most,
            the longest RAW dependence is within `CONST` instructions. 
        """
        if inst.insert_ready_list > inst.dispatch:
            t = i = 0
            dependency = None
            while i < len(self.insts):
                _inst = self.get_inst_via_idx(i)
                if self.raw_dep(inst, _inst):
                    j = 0
                    while j < CONST and i < len(self.insts):
                        """
                            We find the latest instruction that
                            does has the RAW dependence.
                        """
                        if self.raw_dep(inst, _inst):
                            if _inst.is_load:
                                if t < _inst.complete_memory:
                                    t = _inst.complete_memory
                                    dependency = _inst
                            else:
                                if t < _inst.complete:
                                    t = _inst.complete
                                    dependency = _inst
                        j += 1
                        i += 1
                if dependency is not None:
                    break
                i += 1
            if dependency is not None:
                # `dependency` can be None, we ignore if we have this case
                prev_node = self.get_node_via_inst(
                    dependency, PipelineStage.issue
                )
                node = self.get_node_via_inst(
                    inst, PipelineStage.issue
                )
                delay = inst.issue - dependency.issue
                if dependency.is_load:
                    self.add_edge(self.Edge(
                            prev_node, node,
                            delay, delay,
                            Base() \
                                if delay == \
                                    PipelineDelay.dcache_hit_delay.value \
                                else DcacheMiss()
                        )
                    )
                else:
                    self.add_edge(self.Edge(
                            prev_node, node,
                            delay, delay,
                            RAW()
                        )
                    )
                return node

    def model_interaction_impl(self, inst: RiscvInstruction):
        if self.insts.is_empty:
            return
        # dependent_stages = []

        """
            We model interactions based on following types.
        """
        funcs = [
            "miss_bp",
            "rob",
            "lq",
            "sq",
            "rf",
            "iq",
            "fu",
            "raw"
        ]

        def core():
            # nonlocal dependent_stages

            for func in funcs:
                f = getattr(self,
                    "model_{}_interaction".format(func)
                )(inst)
                # dependent_stages.append(f(inst))

        core()

        """
            DEPRECATED.
        """
        # # add meta information, refer it to `model`
        # self.meta.last().add_dependent_stages(
        #         list(filter(None, dependent_stages))
        #     )

    def model_interaction(self, inst: RiscvInstruction):
        edge = self.model_interaction_impl(inst)
        self.insts.insert(inst)

    def add_virtual_edge(self, u, v):
        """
            Connect between the node `u` & `v`.
            We directly connect `u` & `v` with
            a virtual edge.
        """
        if u == v or self.if_exist_edge(u, v):
            return
        else:
            edge = self.Edge(
                u, v,
                v.timestamp - u.timestamp,
                0,
                Virtual(),
                critical=False
            )
            self.add_edge(edge)
            edge.mask_virtual()


    def construct_critical_path_v1(self):
        """
            We construct the critical path with
            the heuristic method.
            1. We sort skew edges according to the delay
            (from the largest to the smallest).
            2. Select non-overlap edges.
            3. Connect them from the head to tail. We
            can add virtual with zero delay.
        """
        info("constructing the critical path...")

        edges = []
        for u, v in self.edges:
            u_node = self.get_node_via_name(u)
            v_node = self.get_node_via_name(v)
            if u_node.seq != v_node.seq:
                edges.append(self.get_edge(u, v))

        # sort according to `delay` in descending order
        edges.sort(key=lambda edge: edge.delay, reverse=True)

        def select_edge(candidate: List[Graph.Edge], edge: Graph.Edge):
            if len(candidate) == 0:
                candidate.append(edge)
                return

            start = edge.u.seq
            end = edge.v.seq

            def is_overlap(_candidate: Graph.Edge):
                s = _candidate.u.seq
                e = _candidate.v.seq
                if start >= e or end <= s:
                    """
                        If two candidates have the same start
                        timestamp or the end timestamp, we
                        determine that they are non-overlapped.
                        Furthermore, they should also be
                        non-overlapped w.r.t. time.
                    """
                    u_timestamp = edge.u.timestamp
                    v_timestamp = edge.v.timestamp
                    _u_timestamp = _candidate.u.timestamp
                    _v_timestamp = _candidate.v.timestamp
                    if _u_timestamp >= v_timestamp or \
                        _v_timestamp <= u_timestamp:
                        return False
                return True

            for _candidate in candidate:
                """
                    Check if `_candidate` has a overlap with
                    `[start, end]`. We select the edge with
                    larger delay to construct the critical path.
                """
                if is_overlap(_candidate):
                    return

            candidate.append(edge)

        """
            Choose edge(s) that are non-overlapped.
            `candidate` records edge(s) that are on
            the critical path.
        """
        candidate = []
        for edge in edges:
            select_edge(
                candidate,
                edge
            )

        for edge in candidate:
            edge.set_critical()

        """
            Connect the gaps between selected edge(s).
        """
        # sort according to the instruction sequence
        candidate.sort(key=lambda edge: edge.u.seq)

        agent = self.source_node
        for edge in candidate:
            if agent.seq <= edge.u.seq:
                self.add_virtual_edge(agent, edge.u)
                agent = edge.v
            else:
                # `seq` > `edge.u.seq`
                raise RuntimeError(
                    "no gaps between {} and {}".format(
                        agent.seq, edge.u.seq
                    )
                )
        self.add_virtual_edge(agent, self.sink_node)

    # def get_first_dependent_node(self, seq: int):
    #     """
    #         DEPRECATED.
    #     """
    #     meta = self.get_meta_via_seq(seq)
    #     return meta.first_dependent_stage

    # def add_virtual_edge_via_seq_v1_impl(self, edge: Edge):
    #     """
    #         DEPRECATED.
    #     """
    #     v = edge.v
    #     if v.seq < self.num_of_inst:
    #         # `v` is not the last instruction
    #         for seq in range(v.seq + 1, self.num_of_inst):
    #             """
    #                 We search the next skewed edge via
    #                 traversing the sequence number.
    #                 `get_first_dependent_node` returns the edge
    #                 with the smallest timestamp if a cross
    #                 instruction edge exists.
    #             """
    #             node = self.get_first_dependent_node(seq)
    #             if node:
    #                 if v.timestamp <= node.u.timestamp:
    #                     self.add_virtual_edge(v, node)
    #                     return

    def add_virtual_edge_via_seq_v2_impl(
        self, idx: int, edge: Edge, skew_edges: List[Edge]
    ):
        def _add_virtual_edge_via_seq_v2_impl(v):
            if idx + 1 < len(skew_edges):
                for _edge in skew_edges[idx + 1:]:
                    if _edge.u.seq > v.seq and \
                        _edge.u.timestamp >= v.timestamp:
                        self.add_virtual_edge(v, _edge.u)
                        return

        _add_virtual_edge_via_seq_v2_impl(edge.u)
        _add_virtual_edge_via_seq_v2_impl(edge.v)

    def add_virtual_edge_via_seq_v2(self, skew_edges: List[Edge]):
        # sort according to `seq`
        skew_edges.sort(key=lambda edge: edge.u.seq)
        threads = []
        for idx in range(len(skew_edges)):
            thread = WorkerThread(
                func=self.add_virtual_edge_via_seq_v2_impl,
                args=(idx, skew_edges[idx], skew_edges,)
            )
            thread.start()
            threads.append(thread)
        # for thread in threads:
        #     thread.join()

        # add head to tail virtual edges
        # head
        node = self.source_node
        for edge in skew_edges:
            if edge.u.seq > node.seq:
                self.add_virtual_edge(node, edge.u)
                break
        # tail
        node = self.sink_node
        edge = skew_edges[-1]
        seq = edge.v.seq
        self.add_virtual_edge(edge.v, node)
        for i in range(len(skew_edges) - 2, -1, -1):
            edge = skew_edges[i]
            if edge.v.seq == seq:
                self.add_virtual_edge(edge.v, node)
            else:
                return


    def add_virtual_edge_via_timestamp_impl(
        self, idx: int, edge: Edge, skew_edges: List[Edge]
    ):
        """
            We find nodes with cross instruction edge, whose
            timestamp is the closest to `edge.v`
        """

        def _add_virtual_edge_via_timestamp_impl(v):
            if idx + 1 < len(skew_edges):
                _idx = idx + 1
                early_timestamp = v.timestamp
                for i in range(idx + 1, len(skew_edges)):
                    # we iterate `skew_edges` util we
                    # find the next instruction whose
                    # `u.seq` is greater than `v.seq`.
                    u = skew_edges[i].u
                    if u.timestamp >= v.timestamp and \
                        u.seq > v.seq:
                        # we do not add the virtual edge
                        # if `u.seq == v.seq` since they
                        # are already connected.
                        self.add_virtual_edge(v, u)
                        early_timestamp = u.timestamp
                        _idx = i
                        break
                # we continue to find if there exist
                # instructions whose `u.timestamp` is
                # equal to the one we have found just now.
                for i in range(_idx + 1, len(skew_edges)):
                    u = skew_edges[i].u
                    if u.seq > v.seq and \
                        early_timestamp == u.timestamp:
                        self.add_virtual_edge(v, u)
                    else:
                        break

        _add_virtual_edge_via_timestamp_impl(edge.u)
        _add_virtual_edge_via_timestamp_impl(edge.v)

    def add_virtual_edge_via_timestamp(self, skew_edges: List[Edge]):
        # sort according to `timestamp`
        skew_edges.sort(key=lambda edge: edge.u.timestamp)
        threads = []
        for idx in range(len(skew_edges)):
            thread = WorkerThread(
                func=self.add_virtual_edge_via_timestamp_impl,
                args=(idx, skew_edges[idx], skew_edges,)
            )
            thread.start()
            threads.append(thread)

            # # if a node is dependent on `skew_edges[idx].v`,
            # # we do not consider to add virtual edges for
            # # the edge.
            # v = skew_edges[idx].v
            # if len(self.out_edges(v.name)) < 2:
            #     thread = WorkerThread(
            #         func=self.add_virtual_edge_via_timestamp_impl,
            #         args=(idx, skew_edges[idx], skew_edges,)
            #     )
            #     thread.start()
            #     threads.append(thread)

        # for thread in threads:
        #     thread.join()

        # add head to tail virtual edges
        # head
        """
            Refer to `add_virtual_edge_via_timestamp_impl`.
        """
        node = self.source_node
        for i in range(len(skew_edges)):
            u = skew_edges[i].u
            _idx = 0
            early_timestamp = node.timestamp
            if u.seq > node.seq:
                self.add_virtual_edge(node, u)
                early_timestamp = u.timestamp
                _idx = i
                break
            for i in range(_idx + 1, len(skew_edges)):
                u = skew_edges[i].u
                if u.seq > node.seq and \
                    early_timestamp == u.timestamp:
                    self.add_virtual_edge(v, u)
                else:
                    break

        # tail
        node = self.sink_node
        edge = skew_edges[-1]
        timestamp = edge.v.timestamp
        self.add_virtual_edge(edge.v, node)
        for i in range(len(skew_edges) - 2, -1, -1):
            edge = skew_edges[i]
            if edge.v.timestamp == timestamp:
                self.add_virtual_edge(edge.v, node)
            else:
                return

    def construct_induced_graph(self):
        """
            At first, we find out all skewed edges.
        """
        skew_edges = []
        for u, v in self.edges:
            edge = self.get_edge(u, v)
            if edge.u.seq != edge.v.seq:
                skew_edges.append(edge)

        """
            For each skewed edge, we add virtual edge
            to connect them. The rule to add virtual
            edge is: for each skewed edge's `v` node,
            we connect the next skewed edge's `u` node.
            The next skewed edge should satisfy some
            rules as shown below:
            1. We select all next skewed edges whose
            `seq` are the closest to the current skewed
            edge.
            2. We select all next skewed edges whose
            `u`'s timestamp are the closest to the current
            skewed edge.
            We can parallelize the virtual edge addition.
        """
        threads = []
        thread = WorkerThread(
            func=self.add_virtual_edge_via_timestamp,
            args=(deepcopy(skew_edges),)
        )
        thread.start()
        threads.append(thread)

        thread = WorkerThread(
            func=self.add_virtual_edge_via_seq_v2,
            args=(deepcopy(skew_edges),)
        )
        thread.start()
        threads.append(thread)

        for thread in threads:
            thread.join()

    def longest_path_impl(self):

        def dag_longest_path(graph):
            dist = {}

            for node in nx.topological_sort(graph):
                pairs = []
                for v in self.pred(node):
                    edge = self.get_edge(v, node)
                    pairs.append((dist[v][0] + edge.cost, v))
                if pairs:
                    dist[node] = max(pairs)
                else:
                    dist[node] = (0, node)
            node, (length, _) = max(dist.items(), key=lambda x: x[1])
            path = []

            while length > 0:
                path.append(node)
                length, node = dist[node]
            return list(reversed(path))

        path = dag_longest_path(self.graph)

        """
            We need to directly add head & tail.
            If no path exists, we construct the virtual edge.
        """
        try:
            head_path = nx.shortest_path(self.graph,
                self.source_node.name,
                path[0])
            head_path = head_path[::-1]
            for n in head_path[1:]:
                path.insert(0, n)
        except nx.exception.NetworkXNoPath:
            self.add_virtual_edge(
                self.source_node,
                self.get_node_via_name(path[0])
            )
            path.insert(0, self.source_node.name)

        try:
            tail_path = nx.shortest_path(
                self.graph,
                path[-1],
                self.sink_node.name
            )
            for n in tail_path[1:]:
                path.insert(len(path), n)
        except nx.exception.NetworkXNoPath:
            self.add_virtual_edge(
                self.get_node_via_name(path[-1]),
                self.sink_node
            )
            path.insert(len(path), self.sink_node.name)
        return path

    def longest_path(self):
        """
            We apply the longest path on the induced graph.
        """
        # assert nx.has_path(self.graph,
        #     self.source_node.name,
        #     self.sink_node.name), \
        #     assert_error("graph is not connected.")

        with Timer("apply longest path (core)"):
            self.critical_path = self.longest_path_impl()

        # set critical edges
        with Timer("mark critical edges"):
            l = len(self.critical_path)
            for i in range(l - 1):
                j = i + 1
                edge = self.get_edge(
                    self.critical_path[i],
                    self.critical_path[j]
                )
                edge.set_critical()
                self.bottleneck[edge.bottleneck.name] += \
                    edge.delay

    def construct_critical_path_v2(self):
        """
            We apply a dynamic programming method
            to construct the graph. The idea is
            inspired from ICCAD'13.
            "Methodology for standard cell compliance and detailed placement for triple patterning lithography"
            We apply the longest path algorithm to
            construct the critical path. Several
            steps are shown below:
            1. Add virtual edges for
            cross-instruction dependencies. The
            virtual edges are added following the
            timestamp. We call this step as
            "contructing an induced DEG".
            2. Apply the longest path algorithm
            in the new graph.
            Ais assigned for each edge:
            1. horizontal edge: 0
            2. virual edge: 0
            3. cross-instruction dependence: `delay`
        """
        info("constructing the critical path...")
        with Timer("construct induced graph"):
            self.construct_induced_graph()
        with Timer("apply longest path"):
            self.longest_path()

    def generate_report(self, output: str):
        critical_path = getattr(self, "critical_path", None)
        if critical_path is None:
            warn("no critical path is constructed.")
            return

        length = 0
        for k, v in self.bottleneck.items():
            length += v
        golden_length = self.sink_node.timestamp - \
            self.source_node.timestamp
        assert golden_length == length, \
            assert_error("golden length: {} vs. length: {}".format(
                    golden_length,
                    length
                )
            )

        with open(output, 'w') as f:
            f.write("critical path: {}\n".format(length))
            l = len(self.critical_path)
            for i in range(l - 1):
                j = i + 1
                edge = self.get_edge(
                    self.critical_path[i],
                    self.critical_path[j]
                )
                msg = "{}\t=>\t{}: {}\t{}".format(
                        edge.u.name, edge.v.name,
                        edge.bottleneck.name,
                        edge.delay
                    )
                f.write("{}\n".format(msg))
            f.write("\nbottleneck:\n")
            msg = ""
            for k, v in self.bottleneck.items():
                msg += "{}: {}\n".format(k, v)
            f.write(msg)
            info("generating report: {}".format(output))
