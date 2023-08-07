# Author: baichen.bai@alibaba-inc.com


from abc import ABC, abstractmethod
from collections import OrderedDict


class Instruction(ABC):
    def __init__(self, inst):
        super(Instruction, self).__init__()
        self._inst = inst

    @property
    def inst(self):
        return self._inst

    @inst.setter
    def inst(self, inst):
        self._inst = inst

    @staticmethod
    def parse(self):
        raise NotImplementedError()


class RiscvInstructionType(object):
    """
        Load instructions.
    """
    __LD__ = ["MemRead", "FloatMemRead"]

    """
        Store instructions.
    """
    __ST__ = ["MemWrite", "FloatMemWrite"]

    """
        Memory-related instructions.
    """
    __MEM__ = __LD__ + __ST__

    """
        Control-related instructions.
    """
    __CTRL__ = ["beq", "bne", "bltu", "blt", \
            "bgeu", "bge", "jalr", "jal", "c_beqz"
        ]

    """
        Instructions that use integer ALU(s).
    """
    __INT_ALU__ = ["IntAlu"]

    """
        Instructions that use integer Mult(s) & Div(s).
    """
    __INT_MULT_DIV__ = ["IntMult", "IntDiv"]

    """
        Instructions that use floating-point ALU(s).
    """
    __FP_ALU__ = ["FloatAdd", "FloatCmp", "FloatCvt"]

    """
        Instructions that use floating-point
        Mult(s) & Div(s).
    """
    __FP_MULT_DIV__ = ["FloatMult", "FloatMultAcc", \
            "FloatMisc", "FloatDiv", "FloatSqrt"
        ]

    """
        Instructions that use read/write ports.
    """
    __RD_WR_PORT__ = ["MemRead", "MemWrite", \
            "FloatMemRead", "FloatMemWrite"
        ]

    """
        Instructions that use integer registers.
    """
    __INT_RF__ = __INT_ALU__ + __INT_MULT_DIV__ + \
        __RD_WR_PORT__

    """
        Instructions that use floating-point registers.
    """
    __FP_RF__ = __FP_ALU__ + __FP_MULT_DIV__


class RiscvInstruction(Instruction, RiscvInstructionType):

    class Cycle(object):
        def __init__(self, inst):
            super(RiscvInstruction.Cycle, self).__init__()
            self.fetch_cache_line = 0
            self.process_cache_completion = 0
            self.fetch = 0
            self.decode_sort_insts = 0
            self.decode = 0
            self.rename_sort_insts = 0
            self.block_from_rob = 0
            self.block_from_rf = 0
            self.block_from_iq = 0
            self.block_from_lq = 0
            self.block_from_sq = 0
            self.rename = 0
            self.dispatch = 0
            self.insert_ready_list = 0
            self.issue = 0
            self.memory = 0
            self.complete = 0
            self.complete_memory = 0
            self.commit_head = 0
            self.commit = 0
            self.construct_tick(inst)

        def tick_to_cycle(self, tick: int):
            return round(tick / 1000)

        def extract_tick(self, inst: str, idx: int):
            return self.tick_to_cycle(
                int(inst[idx].split('=')[1].strip()) 
            )

        def construct_tick(self, inst):
            f = lambda idx: self.extract_tick(inst, idx)
            self.fetch_cache_line = f(7)
            self.process_cache_completion = f(8)
            self.fetch = f(9)
            self.decode_sort_insts = f(10)
            self.decode = f(11)
            self.rename_sort_insts = f(12)
            self.block_from_rob = f(13)
            self.block_from_rf = f(14)
            self.block_from_iq = f(15)
            self.block_from_lq = f(16)
            self.block_from_sq = f(17)
            self.rename = f(18)
            self.dispatch = f(19)
            self.insert_ready_list = f(20)
            self.issue = f(21)
            self.memory = f(22)
            self.complete = f(23)
            self.complete_memory = f(24)
            self.commit_head = f(25)
            self.commit = f(26)


    def __init__(self, seq: int, inst: str):
        super(RiscvInstruction, self).__init__(inst)
        self.seq = seq
        self._inst_type = None
        self._cycle = None
        self._rob = None
        self._lq = None
        self._sq = None
        self._iq = None
        self._fu = None
        self._src = None
        self._dst = None

    @property
    def inst_type(self):
        return self._inst_type

    @inst_type.setter
    def inst_type(self, inst_type):
        self._inst_type = inst_type

    @property
    def cycle(self):
        return self._cycle

    @cycle.setter
    def cycle(self, inst):
        self._cycle = self.Cycle(inst)

    @property
    def rob(self):
        return self._rob

    @rob.setter
    def rob(self, rob):
        self._rob = rob

    @property
    def lq(self):
        return self._lq

    @lq.setter
    def lq(self, lq):
        self._lq = lq

    @property
    def sq(self):
        return self._sq

    @sq.setter
    def sq(self, sq):
        self._sq = sq

    @property
    def iq(self):
        return self._iq

    @iq.setter
    def iq(self, iq):
        self._iq = iq

    @property
    def fu(self):
        return self._fu

    @fu.setter
    def fu(self, fu):
        self._fu = fu

    @property
    def src(self):
        return self._src

    @src.setter
    def src(self, src):
        self._src = src

    @property
    def dst(self):
        return self._dst

    @dst.setter
    def dst(self, dst):
        self._dst = dst

    @property
    def fetch_cache_line(self):
        assert self.cycle
        return self.cycle.fetch_cache_line

    @property
    def process_cache_completion(self):
        assert self.cycle
        return self.cycle.process_cache_completion

    @property
    def fetch(self):
        assert self.cycle
        return self.cycle.fetch

    @property
    def decode_sort_insts(self):
        assert self.cycle
        return self.cycle.decode_sort_insts

    @property
    def decode(self):
        assert self.cycle
        return self.cycle.decode

    @property
    def rename_sort_insts(self):
        assert self.cycle
        return self.cycle.rename_sort_insts

    @property
    def block_from_rob(self):
        assert self.cycle
        return self.cycle.block_from_rob

    @property
    def block_from_rf(self):
        assert self.cycle
        return self.cycle.block_from_rf

    @property
    def block_from_iq(self):
        assert self.cycle
        return self.cycle.block_from_iq

    @property
    def block_from_lq(self):
        assert self.cycle
        return self.cycle.block_from_lq

    @property
    def block_from_sq(self):
        assert self.cycle
        return self.cycle.block_from_sq

    @property
    def rename(self):
        assert self.cycle
        return self.cycle.rename

    @property
    def dispatch(self):
        assert self.cycle
        return self.cycle.dispatch

    @property
    def insert_ready_list(self):
        assert self.cycle
        return self.cycle.insert_ready_list

    @property
    def issue(self):
        assert self.cycle
        return self.cycle.issue

    @property
    def memory(self):
        assert self.cycle
        return self.cycle.memory

    @property
    def complete(self):
        assert self.cycle
        return self.cycle.complete

    @property
    def complete_memory(self):
        assert self.cycle
        return self.cycle.complete_memory

    @property
    def commit_head(self):
        assert self.cycle
        return self.cycle.commit_head

    @property
    def commit(self):
        assert self.cycle
        return self.cycle.commit

    @property
    def is_mem(self):
        return self.inst_type in \
            RiscvInstructionType.__MEM__

    @property
    def is_load(self):
        return self.inst_type in \
            RiscvInstructionType.__LD__

    @property
    def is_store(self):
        return self.inst_type in \
            RiscvInstructionType.__ST__

    @property
    def is_control(self):
        return any([self.inst.startswith(ctrl) \
                for ctrl in \
                    RiscvInstructionType.__CTRL__]
            )

    @property
    def use_int_alu(self):
        return self.inst_type in \
            RiscvInstructionType.__INT_ALU__

    @property
    def use_int_mult_div(self):
        return self.inst_type in \
            RiscvInstructionType.__INT_MULT_DIV__

    @property
    def use_fp_alu(self):
        return self.inst_type in \
            RiscvInstructionType.__FP_ALU__

    @property
    def use_fp_mult_div(self):
        return self.inst_type in \
            RiscvInstructionType.__FP_MULT_DIV__

    @property
    def use_rd_wr_port(self):
        return self.inst_type in \
            RiscvInstructionType.__RD_WR_PORT__

    @property
    def use_int_rf(self):
        return self.inst_type in \
            RiscvInstructionType.__INT_RF__

    @property
    def use_fp_rf(self):
        return self.inst_type in \
            RiscvInstructionType.__FP_RF__

    def parse(self):
        inst = self.inst.split(":")
        # set basic properties for `inst`
        self.inst = inst[4].strip()
        self.inst_type = inst[5].strip()
        # set cycle(s) for `inst`
        self.cycle = inst
        # set hardware resource for `inst`
        f = lambda idx: inst[idx].split('=')[1].strip()
        self.rob = f(27)
        self.lq = f(28)
        self.sq = f(29)
        self.iq = f(30)
        self.fu = f(31)
        # set source registers for `inst`
        src = f(32)
        if len(src) == 0:
            self.src = [-1]
        else:
            self.src = [int(i) for i in src.split(',')]
        # set destination registers for `dst`
        dst = f(33)
        if len(dst) == 0:
            self.dst = -1
        else:
            self.dst = int(dst)
        return self


class InstructionStream(ABC):
    def __init__(self, trace):
        self.benchmark = trace
        self.trace = self.load_trace(trace)

    def load_trace(self, filename: str):
        with open(filename, 'r') as f:
            return f.readlines()

    @abstractmethod
    def parse_inst(self):
        raise NotImplementedError()

    def __len__(self):
        return len(self.trace)

    def __iter__(self):
        for inst in self.trace:
            yield self.parse_inst(inst)

    def __getitem__(self, idx: int):
        try:
            return self.trace[idx]
        except IndexError:
            raise IndexError(
                "idx: {} is out of bounds.".format(idx)
            )


class RiscvInstructionStream(InstructionStream):
    def __init__(self, trace):
        super(RiscvInstructionStream, self).__init__(trace)
        """
            The sequence number is indexed from 1.
        """
        self._seq = 1

    @property
    def seq(self):
        seq = self._seq
        self._seq += 1
        return seq

    @seq.setter
    def seq(self, val: int):
        self._seq = val

    def parse_inst(self, inst):
        return RiscvInstruction(self.seq, inst).parse()
