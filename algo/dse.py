# Author: baichen.bai@alibaba-inc.com


import os
import re
import random
import numpy as np
from copy import deepcopy
from abc import ABC, abstractmethod
from typing import List, Tuple, Dict
from utils.thread import WorkerThread
from algo.core.arch_bottleneck import bottleneck, BIdx
from utils.utils import if_exist, info, warn, assert_error
from funcs.sim.o3cpu.o3cpu_simulation import O3CPUSimulation
from funcs.design.o3cpu.o3cpu_design_space import parse_design_space


pat_ppa_rpt = re.compile(r"\d+\.\d+")



class SensitiveComponent(ABC):
    """
        A naive heuristic method: we record the
        configuration trace & contribution trace
        of sensitive components. We calculate the
        average mean value. If we find the value
        is not improved, we directly set the
        configuration with the lowest contribution.
    """
    def __init__(self):
        super(SensitiveComponent, self).__init__()
        self.terminate = False
        self.configs = []
        self.contribution = []
        self.last = 0

    @abstractmethod
    def set_config(self, embedding: List[int]):
        raise NotImplementedError()

    @abstractmethod
    def fallback(self, embedding: List[int]):
        raise NotImplementedError()

    def update(self, contribution: int):
        self.contribution.append(contribution)

    def update_terminate(self):
        average = np.average(self.contribution)
        if self.last == 0:
            self.last = average
        elif self.last >= average:
            self.last = average
        else:
            self.terminate = True

    def reset(self):
        self.configs.clear()
        self.contribution.clear()
        self.terminate = False
        self.last = 0


class BP(SensitiveComponent):
    def __init__(self):
        super(BP, self).__init__()

    def set_config(self, embedding: List[int]):
        self.configs.append(
            (embedding[3], embedding[4], embedding[5])
        )

    def fallback(self, embedding: List[int]):
        assert len(self.contribution) == len(self.configs), \
            assert_error("contribution: {} != configs: {}".format(
                    len(self.contribution), len(self.configs)
                )
            )
        idx = self.contribution.index(min(self.contribution))
        lp, gp, cp = self.configs[idx]
        embedding[3] = lp
        embedding[4] = gp
        embedding[5] = cp


class ICache(SensitiveComponent):
    def __init__(self):
        super(ICache, self).__init__()

    def set_config(self, embedding: List[int]):
        self.configs.append(
            (embedding[18], embedding[19])
        )

    def fallback(self, embedding: List[int]):
        assert len(self.contribution) == len(self.configs), \
            assert_error("contribution: {} != configs: {}".format(
                    len(self.contribution), len(self.configs)
                )
            )
        idx = self.contribution.index(min(self.contribution))
        size, assoc = self.configs[idx]
        embedding[18] = size
        embedding[19] = assoc


class Dcache(SensitiveComponent):
    def __init__(self):
        super(Dcache, self).__init__()

    def set_config(self, embedding: List[int]):
        self.configs.append(
            (embedding[20], embedding[21])
        )

    def fallback(self, embedding: List[int]):
        assert len(self.contribution) == len(self.configs), \
            assert_error("contribution: {} != configs: {}".format(
                    len(self.contribution), len(self.configs)
                )
            )
        idx = self.contribution.index(min(self.contribution))
        size, assoc = self.configs[idx]
        embedding[20] = size
        embedding[21] = assoc


class SensitiveComponentManager(object):
    """
        D$, I$ and BP are sensitive components.
        If we find little PPA improvement is achieved,
        we terminate adjusting them.
    """    
    def __init__(self):
        super(SensitiveComponentManager, self).__init__()
        self.bp = BP()
        self.icache = ICache()
        self.dcache = Dcache()

    def set_config(self, embedding: List[int]):
        if not self.bp.terminate:
            self.bp.set_config(embedding)
        if not self.icache.terminate:
            self.icache.set_config(embedding)
        if not self.dcache.terminate:
            self.dcache.set_config(embedding)

    def update(self, contribution: Dict):
        if not self.bp.terminate:
            self.bp.update(contribution[
                    bottleneck[BIdx.BPMiss.value]
                ]
            )
        if not self.icache.terminate:
            self.icache.update(contribution[
                    bottleneck[BIdx.IcacheMiss.value]
                ]
            )
        if not self.dcache.terminate:
            self.dcache.update(contribution[
                    bottleneck[BIdx.DcacheMiss.value]
                ]
            )

    def update_terminate(self):
        self.bp.update_terminate()
        self.icache.update_terminate()
        self.dcache.update_terminate()

    def fallback_bp_config(self, embedding: List[int]):
        self.bp.fallback(embedding)

    def fallback_icache_config(self, embedding: List[int]):
        self.icache.fallback(embedding)

    def fallback_dcache_config(self, embedding: List[int]):
        self.dcache.fallback(embedding)

    def reset(self):
        self.bp.reset()
        self.icache.reset()
        self.dcache.reset()


class ArchExplorerEngineMacros(object):
    def __init__(self):
        super(ArchExplorerEngineMacros, self).__init__()
        self.macros = {}
        self.macros["arch-explorer-root"] = \
            os.path.abspath(
                os.path.join(
                    os.path.dirname(__file__),
                    os.path.pardir
                )
            )
        self.macros["temp-root"] = os.path.join(
            self.macros["arch-explorer-root"],
            "temp"
        )
        

class ArchExplorerEngine(ArchExplorerEngineMacros):
    def __init__(self, configs):
        super(ArchExplorerEngine, self).__init__()
        self.configs = configs
        self.design_space = self.init_design_space()
        self.simulator = None
        # we adjust `top_k` hardware resources
        self.top_k = self.dse_configs["top-k"]
        self.scm = SensitiveComponentManager()
        self.init_random_seed()

    def init_random_seed(self):
        random.seed(self.dse_configs["seed"])
        np.random.seed(self.dse_configs["seed"])

    @property
    def dse_configs(self):
        return self.configs["dse"]

    @property
    def pipeline_width(self):
        return self.dse_configs["pipeline-width"]

    @property
    def epoch(self):
        return self.dse_configs["epoch"]

    @property
    def initial_design(self):
        return self.dse_configs["initial-design"]

    @property
    def budget(self):
        return self.dse_configs["budget"]

    @property
    def early_stopping(self):
        return self.dse_configs["early-stopping"]

    @property
    def output(self):
        return os.path.join(self.dse_configs["output"])

    def if_no_need_simulate(self, idx) -> bool:
        simulator_root = self.get_simulator_root(idx)
        if if_exist(simulator_root):
            for k, v in self.simulator.benchmark.macros.items():
                bottleneck_rpt = os.path.join(
                    simulator_root,
                    k,
                    "analysis.rpt"
                )
                if not if_exist(bottleneck_rpt):
                    return False
            # all benchmarks have been verified
            return True
        return False

    def init_design_space(self):
        return parse_design_space(
            self.configs["design-space"]
        )

    def init_simulator(self, pipeline_width: int):
        # self.configs["simulation"]["simulator"]["gem5-research-root"] = \
        #     os.path.join(
        #         self.configs["simulation"]["simulator"]["gem5-research-pool-root"],
        #         str(pipeline_width),
        #         "gem5-research"
        #     )
        return O3CPUSimulation(
            design_space=self.design_space,
            configs=self.configs["simulation"]
        )

    def metric(self, ipc, power, area):
        return (ipc ** 2) / (power * area)

    def get_simulator_root(self, idx):
        return os.path.join(
            self.macros["temp-root"],
            "gem5-{}".format(idx)
        )

    def get_pipeline_width(self, idx: int):
        embedding = self.design_space.idx_to_embedding(idx)
        return embedding[0]

    def generate_baseline_v1(self, pipeline_width):
        """
            TODO: how to generate the baseline?
        """
        embedding = [pipeline_width]
        if pipeline_width == 1:
            embedding += [
                # fetchBufferSize
                16,
                # fetchQueueSize
                random.sample([8, 12, 16], k=1)[0],
                # localPredictorSize
                # globalPredictorSize
                # choicePredictorSize
                512, 2048, 2048,
                # RASSize
                random.sample([16, 18, 20], k=1)[0],
                # BTBEntries
                1024,
                # numROBEntries
                random.sample([32, 48, 64], k=1)[0],
                # numPhysIntRegs numPhysFloatRegs
                random.sample([40, 48, 64, 80], k=1)[0],
                random.sample([40, 48, 64, 80], k=1)[0],
                # numIQEntries
                random.sample([16, 24, 32], k=1)[0],
                # LQEntries
                random.sample([20, 24, 28], k=1)[0],
                # SQEntries
                random.sample([20, 24, 28], k=1)[0],
                # IntALU
                random.sample([3, 4], k=1)[0],
                # IntMult_Div/FP_ALU/FP_MultDiv
                1, 1, 1,
                # l1i_size/l1i_assoc
                16, 2,
                # l1d_size/l1d_assoc
                16, 2
            ]
        elif pipeline_width == 2:
            predictor_size = random.sample(
                [2048, 4096], k=1
            )[0]
            embedding += [
                # fetchBufferSize
                random.sample([16, 32], k=1)[0],
                # fetchQueueSize
                random.sample([12, 16, 20], k=1)[0],
                # localPredictorSize
                random.sample([512, 1024], k=1)[0],
                # globalPredictorSize
                predictor_size,
                # choicePredictorSize
                predictor_size,
                # RASSize
                random.sample([16, 18, 20, 22], k=1)[0],
                # BTBEntries
                random.sample([1024, 2048, 4096], k=1)[0],
                # numROBEntries
                random.sample([32, 48, 64, 80], k=1)[0],
                # numPhysIntRegs
                random.sample([40, 48, 64, 80], k=1)[0],
                # numPhysFloatRegs
                random.sample([40, 48, 64, 80], k=1)[0],
                # numIQEntries
                random.sample([16, 24, 32], k=1)[0],
                # LQEntries
                random.sample([20, 24, 28], k=1)[0],
                # SQEntries
                random.sample([20, 24, 28], k=1)[0],
                # IntALU
                random.sample([3, 4], k=1)[0],
                # IntMult_Div/FP_ALU/FP_MultDiv
                1, 1, 1,
                # l1i_size
                random.sample([16, 32], k=1)[0],
                # l1i_assoc
                random.sample([2, 4], k=1)[0],
                # l1d_size
                random.sample([16, 32], k=1)[0],
                # l1d_assoc
                random.sample([2, 4], k=1)[0]
            ]
        elif pipeline_width == 3:
            predictor_size = random.sample(
                [2048, 4096, 8192], k=1
            )[0]
            embedding += [
                # fetchBufferSize
                random.sample([16, 32, 64], k=1)[0],
                # fetchQueueSize
                random.sample([12, 16, 20, 24], k=1)[0],
                # localPredictorSize
                random.sample([512, 1024, 2048], k=1)[0],
                # globalPredictorSize
                predictor_size,
                # choicePredictorSize
                predictor_size,
                # RASSize
                random.sample([16, 18, 20, 22, 24], k=1)[0],
                # BTBEntries
                random.sample([1024, 2048, 4096], k=1)[0],
                # numROBEntries
                random.sample([32, 48, 64, 80, 96, 112], k=1)[0],
                # numPhysIntRegs
                random.sample([40, 48, 64, 80, 96, 112, 128], k=1)[0],
                # numPhysFloatRegs
                random.sample([40, 48, 64, 80, 96, 112, 128], k=1)[0],
                # numIQEntries
                random.sample([16, 24, 32, 40], k=1)[0],
                # LQEntries
                random.sample([20, 24, 28, 32], k=1)[0],
                # SQEntries
                random.sample([20, 24, 28, 32], k=1)[0],
                # IntALU
                random.sample([3, 4, 5], k=1)[0],
                # IntMult_Div/FP_ALU/FP_MultDiv
                1, 1, 1,
                # l1i_size
                random.sample([16, 32, 64], k=1)[0],
                # l1i_assoc
                random.sample([2, 4], k=1)[0],
                # l1d_size
                random.sample([16, 32, 64], k=1)[0],
                # l1d_assoc
                random.sample([2, 4], k=1)[0]
            ]
        elif pipeline_width == 4:
            predictor_size = random.sample(
                [2048, 4096, 8192], k=1
            )[0]
            embedding += [
                # fetchBufferSize
                random.sample([32, 64], k=1)[0],
                # fetchQueueSize
                random.sample([12, 16, 20, 24, 48], k=1)[0],
                # localPredictorSize
                random.sample([512, 1024, 2048], k=1)[0],
                # globalPredictorSize
                predictor_size,
                # choicePredictorSize
                predictor_size,
                # RASSize
                random.sample([16, 18, 20, 22, 24, 26], k=1)[0],
                # BTBEntries
                random.sample([1024, 2048, 4096], k=1)[0],
                # numROBEntries
                random.sample([80, 96, 112, 128, 144, 160], k=1)[0],
                # numPhysIntRegs
                random.sample([80, 96, 112, 128, 144, 160], k=1)[0],
                # numPhysFloatRegs
                random.sample([80, 96, 112, 128, 144, 160], k=1)[0],
                # numIQEntries
                random.sample([24, 32, 40, 48], k=1)[0],
                # LQEntries
                random.sample([24, 28, 32, 36], k=1)[0],
                # SQEntries
                random.sample([24, 28, 32, 36], k=1)[0],
                # IntALU
                random.sample([4, 5, 6], k=1)[0],
                # IntMult_Div
                random.sample([1, 2], k=1)[0],
                # FP_ALU
                random.sample([1, 2], k=1)[0],
                # FP_MultDiv
                random.sample([1, 2], k=1)[0],
                # l1i_size
                random.sample([16, 32, 64], k=1)[0],
                # l1i_assoc
                random.sample([2, 4], k=1)[0],
                # l1d_size
                random.sample([16, 32, 64], k=1)[0],
                # l1d_assoc
                random.sample([2, 4], k=1)[0]
            ]
        else:
            predictor_size = random.sample(
                [2048, 4096, 8192], k=1
            )[0]
            embedding += [
                # fetchBufferSize
                random.sample([32, 64], k=1)[0],
                # fetchQueueSize
                random.sample([12, 16, 20, 24, 48], k=1)[0],
                # localPredictorSize
                random.sample([512, 1024, 2048], k=1)[0],
                # globalPredictorSize
                predictor_size,
                # choicePredictorSize
                predictor_size,
                # RASSize
                random.sample([16, 18, 20, 22, 24, 26], k=1)[0],
                # BTBEntries
                random.sample([2048, 4096], k=1)[0],
                # numROBEntries
                random.sample([96, 112, 128, 144, 160, 176], k=1)[0],
                # numPhysIntRegs
                random.sample([96, 112, 128, 144, 160, 176], k=1)[0],
                # numPhysFloatRegs
                random.sample([96, 112, 128, 144, 160, 176], k=1)[0],
                # numIQEntries
                random.sample([24, 32, 40, 48, 56], k=1)[0],
                # LQEntries
                random.sample([24, 28, 32, 36], k=1)[0],
                # SQEntries
                random.sample([24, 28, 32, 36], k=1)[0],
                # IntALU
                random.sample([4, 5, 6], k=1)[0],
                # IntMult_Div
                random.sample([1, 2], k=1)[0],
                # FP_ALU
                random.sample([1, 2], k=1)[0],
                # FP_MultDiv
                random.sample([1, 2], k=1)[0],
                # l1i_size
                random.sample([16, 32, 64], k=1)[0],
                # l1i_assoc
                random.sample([2, 4], k=1)[0],
                # l1d_size
                random.sample([16, 32, 64], k=1)[0],
                # l1d_assoc
                random.sample([2, 4], k=1)[0]
            ]

        return self.design_space.embedding_to_idx(embedding)

    def generate_baseline_v2(self, pipeline_width):
        """
            We use ISCA submission's initial designs.
        """
        if pipeline_width == 1:
            candidates = [
                124458700657, 194633611153, 875478822793
            ]
        elif pipeline_width == 2:
            candidates = [
                523948833724858, 449142812403178, 697485874213714
            ]
        elif pipeline_width == 3:
            candidates = [
                399245277562915, 224137884543499, 75801677215747,
                399228607216603, 149722381377187, 250908760829323,
                547881197608387
            ]
        elif pipeline_width == 4:
            candidates = [
                583789556875388, 287596362119732, 460258461986780,
                460613238994508, 743513602906604, 393337325962668
            ]
        elif pipeline_width == 5:
            candidates = [
                583789636473765, 287596441718109, 460258541585157,
                315391499100069, 743513682504981, 393337422055237
            ]
        elif pipeline_width == 6:
            candidates = [
                583789636473766, 287596441718110, 291781820609518,
                315391499100070, 743513682504982, 393337422055238
            ]
        elif pipeline_width == 7:
            candidates = [
                583789636473767, 287596441718111, 460258541585159,
                315391499100071, 743513682504983, 393337422055239
            ]
        elif pipeline_width == 8:
            candidates = [
                583789636473768, 287596441718112, 460258541585160,
                315391499100072, 743513682504984, 393337422055240
            ]

        return candidates[i]

    def get_simulation_results(self, idx):
        simulator_root = self.get_simulator_root(idx)
        ipc, cpi, power, area = [], [], [], []
        for k, v in self.simulator.benchmark.macros.items():
            """
                "ppa.rpt" could be failed to generate due to
                the failed simulation.
            """
            ppa_rpt = os.path.join(
                simulator_root,
                k,
                "ppa.rpt"
            )
            if if_exist(ppa_rpt):
                with open(ppa_rpt, 'r') as fin:
                    result = fin.read()
                    result = re.findall(pat_ppa_rpt, result)
                    _ipc = float(result[0])
                    _cpi = float(result[1])
                    _power = float(result[2])
                    _area = float(result[3])
                    ipc.append(_ipc)
                    cpi.append(_cpi)
                    power.append(_power)
                    area.append(_area)
        if len(ipc) == 0:
            return 0, 0, 0, 0
        ipc = np.average(ipc)
        cpi = np.average(cpi)
        power = np.average(power)
        area = np.average(area)
        return ipc, cpi, power, area

    def evaluate_microarch(self, idx):
        if self.if_no_need_simulate(idx):
            return self.get_simulation_results(idx)
        embedding = \
            self.design_space.idx_to_embedding(idx)
        self.simulator.simulate(embedding)
        return self.get_simulation_results(idx)

    def get_idx(self, name, value):
        for k, v in \
            self.design_space.components_mappings[name].items():
            if v[0] == value:
                return int(k)
        return -1

    def adjust_icache(self, embedding: List[int], up: bool = False):
        adjust = False
        size = embedding[18]
        assoc = embedding[19]
        tot_size = len(
                self.design_space.components_mappings["l1i_size"].keys()
            ) - 1
        tot_assoc = len(
                self.design_space.components_mappings["l1i_assoc"].keys()
            ) - 1

        if up:
            # increase
            idx = self.get_idx("l1i_size", size)
            if idx != -1 and tot_size != idx:
                embedding[18] = \
                    self.design_space.components_mappings["l1i_size"][idx + 1][0]
                adjust = True
            idx = self.get_idx("l1i_assoc", assoc)
            if idx != -1 and tot_assoc != idx:
                embedding[19] = \
                    self.design_space.components_mappings["l1i_assoc"][idx + 1][0]
                adjust = True
        else:
            # decrease
            idx = self.get_idx("l1i_size", size)
            if idx != -1 and idx != 1:
                embedding[18] = \
                    self.design_space.components_mappings["l1i_size"][idx - 1][0]
                adjust = True
            idx = self.get_idx("l1i_assoc", assoc)
            if idx != -1 and idx != 1:
                embedding[19] = \
                    self.design_space.components_mappings["l1i_assoc"][idx - 1][0]
                adjust = True
        return adjust

    def adjust_dcache(self, embedding: List[int], up: bool = False):
        adjust = False
        size = embedding[20]
        assoc = embedding[21]
        tot_size = len(
                self.design_space.components_mappings["l1d_size"].keys()
            ) - 1
        tot_assoc = len(
                self.design_space.components_mappings["l1d_assoc"].keys()
            ) - 1

        if up:
            # increase
            idx = self.get_idx("l1d_size", size)
            if idx != -1 and tot_size != idx:
                embedding[20] = \
                    self.design_space.components_mappings["l1d_size"][idx + 1][0]
                adjust = True
            idx = self.get_idx("l1d_assoc", assoc)
            if idx != -1 and tot_assoc != idx:
                embedding[21] = \
                    self.design_space.components_mappings["l1d_assoc"][idx + 1][0]
                adjust = True
        else:
            # decrease
            idx = self.get_idx("l1d_size", size)
            if idx != -1 and idx != 1:
                embedding[20] = \
                    self.design_space.components_mappings["l1d_size"][idx - 1][0]
                adjust = True
            idx = self.get_idx("l1d_assoc", assoc)
            if idx != -1 and idx != 1:
                embedding[21] = \
                    self.design_space.components_mappings["l1d_assoc"][idx - 1][0]
                adjust = True
        return adjust

    def adjust_bp(self, embedding: List[int], up: bool = False):
        adjust = False
        lp = embedding[3]
        gp = embedding[4]
        cp = embedding[5]
        ras = embedding[6]
        btb = embedding[7]

        tot_lp = len(
            self.design_space.components_mappings["localPredictorSize"].keys()
        ) - 1
        tot_gp = len(
            self.design_space.components_mappings["globalPredictorSize"].keys()
        ) - 1
        tot_cp = len(
            self.design_space.components_mappings["choicePredictorSize"].keys()
        ) - 1
        tot_ras = len(
            self.design_space.components_mappings["RASSize"].keys()
        ) - 1
        tot_btb = len(
            self.design_space.components_mappings["BTBEntries"].keys()
        ) - 1

        if up:
            # increase
            idx = self.get_idx("localPredictorSize", lp)
            if idx != -1 and tot_lp != idx:
                embedding[3] = \
                    self.design_space.components_mappings["localPredictorSize"][idx + 1][0]
                adjust = True
            idx = self.get_idx("globalPredictorSize", gp)
            if idx != -1 and tot_gp != idx:
                embedding[4] = \
                    self.design_space.components_mappings["globalPredictorSize"][idx + 1][0]
                adjust = True
            idx = self.get_idx("choicePredictorSize", cp)
            if idx != -1 and tot_cp != idx:
                embedding[5] = \
                    self.design_space.components_mappings["globalPredictorSize"][idx + 1][0]
                adjust = True
            idx = self.get_idx("RASSize", ras)
            if idx != -1 and tot_ras != idx:
                embedding[6] = \
                    self.design_space.components_mappings["RASSize"][idx + 1][0]
                adjust = True
            idx = self.get_idx("BTBEntries", btb)
            if idx != -1 and tot_btb != idx:
                embedding[7] = \
                    self.design_space.components_mappings["BTBEntries"][idx + 1][0]
                adjust = True
        else:
            # decrease
            idx = self.get_idx("localPredictorSize", lp)
            if idx != -1 and idx != 1:
                embedding[3] = \
                    self.design_space.components_mappings["localPredictorSize"][idx - 1][0]
                adjust = True
            idx = self.get_idx("globalPredictorSize", gp)
            if idx != -1 and idx != 1:
                embedding[4] = \
                    self.design_space.components_mappings["globalPredictorSize"][idx - 1][0]
                adjust = True
            idx = self.get_idx("choicePredictorSize", cp)
            if idx != -1 and idx != 1:
                embedding[5] = \
                    self.design_space.components_mappings["globalPredictorSize"][idx - 1][0]
                adjust = True
            idx = self.get_idx("RASSize", ras)
            if idx != -1 and idx != 1:
                embedding[6] = \
                    self.design_space.components_mappings["RASSize"][idx - 1][0]
                adjust = True
            idx = self.get_idx("BTBEntries", btb)
            if idx != -1 and idx != 1:
                embedding[7] = \
                    self.design_space.components_mappings["BTBEntries"][idx - 1][0]
                adjust = True
        return adjust

    def adjust_rob(self, embedding: List[int], up: bool = False):
        adjust = False
        rob = embedding[8]
        tot_rob = len(
                self.design_space.components_mappings["numROBEntries"].keys()
            ) - 1

        if up:
            # increase
            idx = self.get_idx("numROBEntries", rob)
            if idx != -1 and tot_rob != idx:
                embedding[8] = \
                    self.design_space.components_mappings["numROBEntries"][idx + 1][0]
                adjust = True
        else:
            # decrease
            idx = self.get_idx("numROBEntries", rob)
            if idx != -1 and idx != 1:
                embedding[8] = \
                    self.design_space.components_mappings["numROBEntries"][idx - 1][0]
                adjust = True
        return adjust

    def adjust_lq(self, embedding: List[int], up: bool = False):
        adjust = False
        lq = embedding[12]
        tot_lq = len(
                self.design_space.components_mappings["LQEntries"].keys()
            ) - 1

        if up:
            # increase
            idx = self.get_idx("LQEntries", lq)
            if idx != -1 and tot_lq != idx:
                embedding[12] = \
                    self.design_space.components_mappings["LQEntries"][idx + 1][0]
                adjust = True
        else:
            # decrease
            idx = self.get_idx("LQEntries", lq)
            if idx != -1 and idx != 1:
                embedding[12] = \
                    self.design_space.components_mappings["LQEntries"][idx - 1][0]
                adjust = True
        return adjust

    def adjust_sq(self, embedding: List[int], up: bool = False):
        adjust = False
        sq = embedding[13]
        tot_sq = len(
                self.design_space.components_mappings["SQEntries"].keys()
            ) - 1

        if up:
            # increase
            idx = self.get_idx("SQEntries", sq)
            if idx != -1 and tot_sq != idx:
                embedding[13] = \
                    self.design_space.components_mappings["SQEntries"][idx + 1][0]
                adjust = True
        else:
            # decrease
            idx = self.get_idx("SQEntries", sq)
            if idx != -1 and idx != 1:
                embedding[13] = \
                    self.design_space.components_mappings["SQEntries"][idx - 1][0]
                adjust = True
        return adjust

    def adjust_int_rf(self, embedding: List[int], up: bool = False):
        adjust = False
        int_rf = embedding[9]
        tot_int_rf = len(
                self.design_space.components_mappings["numPhysIntRegs"].keys()
            ) - 1

        if up:
            # increase
            idx = self.get_idx("numPhysIntRegs", int_rf)
            if idx != -1 and tot_int_rf != idx:
                embedding[9] = \
                    self.design_space.components_mappings["numPhysIntRegs"][idx + 1][0]
                adjust = True
        else:
            # decrease
            idx = self.get_idx("numPhysIntRegs", int_rf)
            if idx != -1 and idx != 1:
                embedding[9] = \
                    self.design_space.components_mappings["numPhysIntRegs"][idx - 1][0]
                adjust = True
        return adjust

    def adjust_fp_rf(self, embedding: List[int], up: bool = False):
        adjust = False
        fp_rf = embedding[10]
        tot_fp_rf = len(
                self.design_space.components_mappings["numPhysFloatRegs"].keys()
            ) - 1

        if up:
            # increase
            idx = self.get_idx("numPhysFloatRegs", fp_rf)
            if idx != -1 and tot_fp_rf != idx:
                embedding[10] = \
                    self.design_space.components_mappings["numPhysFloatRegs"][idx + 1][0]
                adjust = True
        else:
            # decrease
            idx = self.get_idx("numPhysFloatRegs", fp_rf)
            if idx != -1 and idx != 1:
                embedding[10] = \
                    self.design_space.components_mappings["numPhysFloatRegs"][idx - 1][0]
                adjust = True
        return adjust

    def adjust_iq(self, embedding: List[int], up: bool = False):
        adjust = False
        iq = embedding[11]
        tot_iq = len(
                self.design_space.components_mappings["numIQEntries"].keys()
            ) - 1

        if up:
            # increase
            idx = self.get_idx("numIQEntries", iq)
            if idx != -1 and tot_iq != idx:
                embedding[11] = \
                    self.design_space.components_mappings["numIQEntries"][idx + 1][0]
                adjust = True
        else:
            # decrease
            idx = self.get_idx("numIQEntries", iq)
            if idx != -1 and idx != 1:
                embedding[11] = \
                    self.design_space.components_mappings["numIQEntries"][idx - 1][0]
                adjust = True
        return adjust

    def adjust_int_alu(self, embedding: List[int], up: bool = False):
        adjust = False
        int_alu = embedding[14]
        tot_int_alu = len(
                self.design_space.components_mappings["IntALU"].keys()
            ) - 1

        if up:
            # increase
            idx = self.get_idx("IntALU", int_alu)
            if idx != -1 and tot_int_alu != idx:
                embedding[14] = \
                    self.design_space.components_mappings["IntALU"][idx + 1][0]
                adjust = True
        else:
            # decrease
            idx = self.get_idx("IntALU", int_alu)
            if idx != -1 and idx != 1:
                embedding[14] = \
                    self.design_space.components_mappings["IntALU"][idx - 1][0]
                adjust = True
        return adjust

    def adjust_int_mult_div(self, embedding: List[int], up: bool = False):
        adjust = False
        int_mult_div = embedding[15]
        tot_int_mult_div = len(
                self.design_space.components_mappings["IntMult_Div"].keys()
            ) - 1

        if up:
            # increase
            idx = self.get_idx("IntMult_Div", int_mult_div)
            if idx != -1 and tot_int_mult_div != idx:
                embedding[15] = \
                    self.design_space.components_mappings["IntMult_Div"][idx + 1][0]
                adjust = True
        else:
            # decrease
            idx = self.get_idx("IntMult_Div", int_mult_div)
            if idx != -1 and idx != 1:
                embedding[15] = \
                    self.design_space.components_mappings["IntMult_Div"][idx - 1][0]
                adjust = True
        return adjust

    def adjust_fp_alu(self, embedding: List[int], up: bool = False):
        adjust = False
        fp_alu = embedding[16]
        tot_fp_alu = len(
                self.design_space.components_mappings["FP_ALU"].keys()
            ) - 1

        if up:
            # increase
            idx = self.get_idx("FP_ALU", fp_alu)
            if idx != -1 and tot_fp_alu != idx:
                embedding[16] = \
                    self.design_space.components_mappings["FP_ALU"][idx + 1][0]
                adjust = True
        else:
            # decrease
            idx = self.get_idx("FP_ALU", fp_alu)
            if idx != -1 and idx != 1:
                embedding[16] = \
                    self.design_space.components_mappings["FP_ALU"][idx - 1][0]
                adjust = True
        return adjust

    def adjust_fp_mult_div(self, embedding: List[int], up: bool = False):
        adjust = False
        fp_mult_div = embedding[17]
        tot_fp_mult_div = len(
                self.design_space.components_mappings["FP_MultDiv"].keys()
            ) - 1

        if up:
            # increase
            idx = self.get_idx("FP_MultDiv", fp_mult_div)
            if idx != -1 and tot_fp_mult_div != idx:
                embedding[17] = \
                    self.design_space.components_mappings["FP_MultDiv"][idx + 1][0]
                adjust = True
        else:
            # decrease
            idx = self.get_idx("FP_MultDiv", fp_mult_div)
            if idx != -1 and idx != 1:
                embedding[17] = \
                    self.design_space.components_mappings["FP_MultDiv"][idx - 1][0]
                adjust = True
        return adjust

    def adjust_rd_wr_port(self, embedding: List[int], up: bool = False):
        # currently, we do not support read/write port
        pass

    def adjust_raw(self, embedding: List[int], up: bool = False):
        # it is due to the benchmark parallelism
        pass

    def increase_hardware_resource(
        self, embedding: List[int], contribution: List[Tuple[str, int]]
    ):
        self.scm.update_terminate()

        num_of_adjust = 0
        for btnk_name, contrib in contribution:
            adjust = False
            if num_of_adjust >= self.top_k:
                break
            if btnk_name == bottleneck[BIdx.Base.value]:
                pass
            elif btnk_name == bottleneck[BIdx.IcacheMiss.value]:
                if not self.scm.icache.terminate:
                    adjust = self.adjust_icache(embedding, True)
                else:
                    self.scm.fallback_icache_config(embedding)
            elif btnk_name == bottleneck[BIdx.DcacheMiss.value]:
                if not self.scm.dcache.terminate:
                    adjust = self.adjust_dcache(embedding, True)
                else:
                    self.scm.fallback_dcache_config(embedding)
            elif btnk_name == bottleneck[BIdx.BPMiss.value]:
                if not self.scm.bp.terminate:
                    adjust = self.adjust_bp(embedding, True)
                else:
                    self.scm.fallback_bp_config(embedding)
            elif btnk_name == bottleneck[BIdx.ROB.value]:
                adjust = self.adjust_rob(embedding, True)
            elif btnk_name == bottleneck[BIdx.LQ.value]:
                adjust = self.adjust_lq(embedding, True)
            elif btnk_name == bottleneck[BIdx.SQ.value]:
                adjust = self.adjust_sq(embedding, True)
            elif btnk_name == bottleneck[BIdx.IntRF.value]:
                adjust = self.adjust_int_rf(embedding, True)
            elif btnk_name == bottleneck[BIdx.FpRF.value]:
                adjust = self.adjust_fp_rf(embedding, True)
            elif btnk_name == bottleneck[BIdx.IQ.value]:
                adjust = self.adjust_iq(embedding, True)
            elif btnk_name == bottleneck[BIdx.IntAlu.value]:
                adjust = self.adjust_int_alu(embedding, True)
            elif btnk_name == bottleneck[BIdx.IntMultDiv.value]:
                adjust = self.adjust_int_mult_div(embedding, True)
            elif btnk_name == bottleneck[BIdx.FpAlu.value]:
                adjust = self.adjust_fp_alu(embedding, True)
            elif btnk_name == bottleneck[BIdx.FpMultDiv.value]:
                adjust = self.adjust_fp_mult_div(embedding, True)
            elif btnk_name == bottleneck[BIdx.RdWrPort.value]:
                adjust = self.adjust_rd_wr_port(embedding, True)
            elif btnk_name == bottleneck[BIdx.RAW.value]:
                adjust = self.adjust_raw(embedding, True)
            elif btnk_name == bottleneck[BIdx.Virtual.value]:
                pass
            if adjust:
                num_of_adjust += 1

    def decrease_hardware_resource_v1(
        self, embedding: List[int], contribution: List[Tuple[str, int]]
    ):
        """
            Decrease hardware resources that have zero contribution.
        """
        for btnk_name, contrib in contribution:
            if contrib == 0:
                if btnk_name == bottleneck[BIdx.Base.value]:
                    pass
                elif btnk_name == bottleneck[BIdx.IcacheMiss.value]:
                    self.adjust_icache(embedding)
                elif btnk_name == bottleneck[BIdx.DcacheMiss.value]:
                    self.adjust_dcache(embedding)
                elif btnk_name == bottleneck[BIdx.BPMiss.value]:
                    self.adjust_bp(embedding)
                elif btnk_name == bottleneck[BIdx.ROB.value]:
                    self.adjust_rob(embedding)
                elif btnk_name == bottleneck[BIdx.LQ.value]:
                    self.adjust_lq(embedding)
                elif btnk_name == bottleneck[BIdx.SQ.value]:
                    self.adjust_sq(embedding)
                elif btnk_name == bottleneck[BIdx.IntRF.value]:
                    self.adjust_int_rf(embedding)
                elif btnk_name == bottleneck[BIdx.FpRF.value]:
                    self.adjust_fp_rf(embedding)
                elif btnk_name == bottleneck[BIdx.IQ.value]:
                    self.adjust_iq(embedding)
                elif btnk_name == bottleneck[BIdx.IntAlu.value]:
                    self.adjust_int_alu(embedding)
                elif btnk_name == bottleneck[BIdx.IntMultDiv.value]:
                    self.adjust_int_mult_div(embedding)
                elif btnk_name == bottleneck[BIdx.FpAlu.value]:
                    self.adjust_fp_alu(embedding)
                elif btnk_name == bottleneck[BIdx.FpMultDiv.value]:
                    self.adjust_fp_mult_div(embedding)
                elif btnk_name == bottleneck[BIdx.RdWrPort.value]:
                    self.adjust_rd_wr_port(embedding)
                elif btnk_name == bottleneck[BIdx.RAW.value]:
                    self.adjust_raw(embedding)
                elif btnk_name == bottleneck[BIdx.Virtual.value]:
                    pass

    def decrease_hardware_resource_v2(
        self, embedding: List[int], contribution: List[Tuple[str, int]]
    ):
        """
            Decrease hardware resources that have zero contribution.
            We randomly decrease `top_k` hardware resources.
        """
        candidates = []
        for btnk_name, contrib in contribution:
            if contrib == 0:
                candidates.append(btnk_name)
        candidates = random.sample(
            candidates, k=self.top_k \
                if self.top_k < len(candidates) else len(candidates)
        )

        for btnk_name in candidates:
            if btnk_name == bottleneck[BIdx.Base.value]:
                pass
            elif btnk_name == bottleneck[BIdx.IcacheMiss.value]:
                self.adjust_icache(embedding)
            elif btnk_name == bottleneck[BIdx.DcacheMiss.value]:
                self.adjust_dcache(embedding)
            elif btnk_name == bottleneck[BIdx.BPMiss.value]:
                self.adjust_bp(embedding)
            elif btnk_name == bottleneck[BIdx.ROB.value]:
                self.adjust_rob(embedding)
            elif btnk_name == bottleneck[BIdx.LQ.value]:
                self.adjust_lq(embedding)
            elif btnk_name == bottleneck[BIdx.SQ.value]:
                self.adjust_sq(embedding)
            elif btnk_name == bottleneck[BIdx.IntRF.value]:
                self.adjust_int_rf(embedding)
            elif btnk_name == bottleneck[BIdx.FpRF.value]:
                self.adjust_fp_rf(embedding)
            elif btnk_name == bottleneck[BIdx.IQ.value]:
                self.adjust_iq(embedding)
            elif btnk_name == bottleneck[BIdx.IntAlu.value]:
                self.adjust_int_alu(embedding)
            elif btnk_name == bottleneck[BIdx.IntMultDiv.value]:
                self.adjust_int_mult_div(embedding)
            elif btnk_name == bottleneck[BIdx.FpAlu.value]:
                self.adjust_fp_alu(embedding)
            elif btnk_name == bottleneck[BIdx.FpMultDiv.value]:
                self.adjust_fp_mult_div(embedding)
            elif btnk_name == bottleneck[BIdx.RdWrPort.value]:
                self.adjust_rd_wr_port(embedding)
            elif btnk_name == bottleneck[BIdx.RAW.value]:
                self.adjust_raw(embedding)
            elif btnk_name == bottleneck[BIdx.Virtual.value]:
                pass

    def bottleneck_removal(self, idx: int, contribution: List[Tuple[str, int]]):
        """
            Adjust hardware resources based on the contribution of
            each bottleneck. We adjust top-2 resources and all
            resources with zero contributions
        """
        # increase hardware resources
        embedding = self.design_space.idx_to_embedding(idx)
        self.increase_hardware_resource(embedding, contribution)
        # decrease hardware resources
        self.decrease_hardware_resource_v2(embedding, contribution)
        return embedding


    def calc_bottleneck_contribution(self, btnks: dict):
        """
            `contribution` is a list of different
            bottlenecks' average contribution(s).
            among all benchmarks.
        """
        contribution = {}
        for btnk in bottleneck:
            contribution[btnk] = 0

        # get the total ratio w.r.t. each bottleneck
        for k, v in btnks.items():
            _contribution = {}
            for btnk_name, contrib in v.items():
                if btnk_name == "length":
                    continue
                if btnk_name not in _contribution.keys():
                    _contribution[btnk_name] = 0
                _contribution[btnk_name] += contrib
            for btnk_name in _contribution.keys():
                _contribution[btnk_name] /= v["length"]
                contribution[btnk_name] += \
                    _contribution[btnk_name]

        # get the average contribution
        num_of_benchmark = len(btnks)
        for k, v in contribution.items():
            contribution[k] = v / num_of_benchmark

        # update the sensitive components' information
        self.scm.update(contribution)

        return contribution

    def get_bottleneck_contribution(self, idx):
        simulator_root = self.get_simulator_root(idx)

        """
            `btnks` saves the bottleneck meta information
            of each benchmark. The meta information is a
            mapping between each type of bottleneck and
            its contribution to the critical path.
        """
        btnks = {}
        for k, v in self.simulator.benchmark.macros.items():
            btnk_rpt = os.path.join(
                simulator_root, k, "analysis.rpt"
            )
            """
                "analysis.rpt" could be failed to generate
                due to the failed simulation.
            """
            if if_exist(btnk_rpt):
                btnk = {}
                with open(btnk_rpt, 'r') as f:
                    cnt = f.readlines()
                    length = int(cnt[0].split(':')[-1])
                    cnt = cnt[-len(bottleneck):]
                    for line in cnt:
                        btnk_name, contrib = line.split(":")
                        btnk[btnk_name] = int(contrib)
                    # we include the critical path length
                    btnk["length"] = length
                btnks[k] = deepcopy(btnk)
        contribution = self.calc_bottleneck_contribution(btnks)
        # sort the contribution from the largest to the smallest
        # `summary` consists of elements:
        # `(bottleneck, average contribution)`
        summary = []
        for k, v in contribution.items():
            summary.append([k, v])
        summary.sort(key=lambda item: item[1], reverse=True)
        return summary

    def bottleneck_analysis(self, idx):
        contribution = self.get_bottleneck_contribution(idx)
        return self.bottleneck_removal(idx, contribution)

    def exploration_impl(self, start: int):
        self.scm.reset()
        if start > 8:
            # a random initialization cannot generate
            # a number within [1, 8]
            pipeline_width = self.get_pipeline_width(start)
            idx = start
        else:
            # a pipeline width
            pipeline_width = start
            idx = self.generate_baseline_v1(start)
        self.simulator = self.init_simulator(pipeline_width)
        last_update = 0
        solutions = {
            "trace": [],
            "best-pppa": 0,
            "best-ppa": (),
            "best-idx": 0,
        }
        self.scm.set_config(self.design_space.idx_to_embedding(idx))
        for i in range(self.budget):
            embedding = self.design_space.idx_to_embedding(idx)
            ipc, cpi, power, area = \
                self.evaluate_microarch(idx)
            cur_pppa = self.metric(ipc, power, area)
            solutions["trace"].append(
                (embedding, ipc, cpi, power, area, cur_pppa)
            )
            if ipc == 0:
                """
                    The simulation is failed. We terminate
                    this epoch.
                """
                return solutions

            if cur_pppa > solutions["best-pppa"]:
                solutions["best-pppa"] = cur_pppa
                solutions["best-ppa"] = (ipc, cpi, power, area)
                solutions["best-idx"] = idx
                last_update = i

            # get the new microarch
            embedding = self.bottleneck_analysis(idx)
            self.scm.set_config(embedding)
            idx = self.design_space.embedding_to_idx(embedding)
            if (i - last_update) >= self.early_stopping:
                info("early stopping is satisfied at {}/{}.".format(
                    i, last_update
                )
            )
                break
        return solutions

    def multi_exploration(self, pipeline_width: int):
        threads = []
        assert self.epoch == 1, assert_error(
                "currently, we only support ont thread " \
                "for each pipeline width."
            )
        for epoch in range(self.epoch):
            thread = WorkerThread(
                func=self.exploration_impl,
                args=(pipeline_width,)
            )
            threads.append(thread)
            thread.start()

        for thread in threads:
            thread.join()

        solutions = []
        for thread in threads:
            solutions.append(thread.get_output())
        return solutions

    def exploration(self, start: int):
        solutions = []
        for epoch in range(self.epoch):
            solutions.append(
                self.exploration_impl(start)
            )
        return solutions

    def run(self):
        """
            For artifact evaluation, we disable parallel
            DSE.
        """
        threads = []
        if isinstance(self.initial_design, list):
            for initial_idx in self.initial_design:
                thread = WorkerThread(
                    func=self.exploration,
                    args=(initial_idx,)
                )
                threads.append(thread)
                thread.start()
                thread.join()
        else:
            for pipeline_width in self.pipeline_width:
                thread = WorkerThread(
                    func=self.exploration,
                    args=(pipeline_width,)
                )
                threads.append(thread)
                thread.start()
                thread.join()

            # for thread in threads:
            #     thread.join()

        solutions = []
        for thread in threads:
            solutions.append(thread.get_output())
        self.generate_report(solutions)

    def generate_report(self, solutions: List[List[Dict]]):
        """
            Refer `solutions` to `exploration` &
            `exploration_impl`.
            E.g.,
                [
                    ## pipeline 1
                    [
                        # epoch 1
                        {
                            "trace": [],
                            "best-pppa": 0,
                            "best-ppa": ()
                            "best-idx": 0,
                        },
                        # epoch 2
                        {
                            "trace": [],
                            "best-pppa": 0,
                            "best-ppa": ()
                            "best-idx": 0,
                        },
                    # ...
                    ]
                    ## pipeline 2
                    [
                        # epoch 1
                        {
                            "trace": [],
                            "best-pppa": 0,
                            "best-ppa": ()
                            "best-idx": 0,
                        },
                        # epoch 2
                        {
                            "trace": [],
                            "best-pppa": 0,
                            "best-ppa": ()
                            "best-idx": 0,
                        },
                    # ...
                    ]
                ]
        """
        idx = 1
        for i in range(len(self.pipeline_width)):
            report_name = "{}-{}".format(self.output, self.pipeline_width[i])
            with open(report_name, 'w') as fout:
                # record the trace
                for solution in solutions[i]:
                    msg = ""
                    best_pppa = best_idx = 0
                    best_ppa = ()
                    for (embedding, ipc, _, power, area, pppa) \
                        in solution["trace"]:
                        msg += "{}: {} {}\tipc: {}\tpower: {}\t" \
                            "area: {}\tpppa: {}\n".format(
                            idx, self.design_space.embedding_to_idx(embedding),
                            embedding, ipc, power, area, pppa
                        )
                        idx += 1
                    idx = 1
                    if solution["best-pppa"] > best_pppa:
                        best_pppa = solution["best-pppa"]
                        best_ppa = solution["best-ppa"]
                        best_idx = solution["best-idx"]
                    if best_idx == 0:
                        warn("an epoch of {} is failed.".format(
                                self.pipeline_width[i]
                            )
                        )
                        continue
                    fout.write("{}\n".format(msg))
                    # record the optimal solution
                    msg = "\noptimal solution: {} {}\tipc: {}\t" \
                        "power: {}\tarea: {}\tpppa: {}\n\n".format(
                        best_idx,
                        self.design_space.idx_to_embedding(best_idx),
                        best_ppa[0],
                        best_ppa[2],
                        best_ppa[3],
                        best_pppa
                    )
                    fout.write(msg)
                    info("generate report: {}".format(report_name))


def archexplorer(configs):
    engine = ArchExplorerEngine(configs)
    engine.run()
