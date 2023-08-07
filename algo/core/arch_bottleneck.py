# Author: baichen.bai@alibaba-inc.com


from enum import Enum
from abc import ABC, abstractmethod


class Bottleneck(ABC):
    def __init__(self, name):
        super(Bottleneck).__init__()
        self.name = name

    def __repr__(self):
        return "{}".format(self.name)


class Base(Bottleneck):
    def __init__(self):
        # pipeline delay
        super(Base, self).__init__("Base")


class IcacheMiss(Bottleneck):
    def __init__(self):
        super(IcacheMiss, self).__init__("I-cache Miss")


class DcacheMiss(Bottleneck):
    def __init__(self):
        super(DcacheMiss, self).__init__("D-cache Miss")


class BPMiss(Bottleneck):
    def __init__(self):
        super(BPMiss, self).__init__("BP Miss")


class ROB(Bottleneck):
    def __init__(self):
        super(ROB, self).__init__("ROB")


class LQ(Bottleneck):
    def __init__(self):
        super(LQ, self).__init__("LQ")


class SQ(Bottleneck):
    def __init__(self):
        super(SQ, self).__init__("SQ")


class IntRF(Bottleneck):
    def __init__(self):
        super(IntRF, self).__init__("INT RF")


class FpRF(Bottleneck):
    def __init__(self):
        super(FpRF, self).__init__("FP RF")


class IQ(Bottleneck):
    def __init__(self):
        super(IQ, self).__init__("IQ")


class IntAlu(Bottleneck):
    def __init__(self):
        super(IntAlu, self).__init__("IntAlu")


class IntMultDiv(Bottleneck):
    def __init__(self):
        super(IntMultDiv, self).__init__("IntMultDiv")


class FpAlu(Bottleneck):
    def __init__(self):
        super(FpAlu, self).__init__("FpAlu")


class FpMultDiv(Bottleneck):
    def __init__(self):
        super(FpMultDiv, self).__init__("FpMultDiv")


class RdWrPort(Bottleneck):
    def __init__(self):
        super(RdWrPort, self).__init__("RdWrPort")


class RAW(Bottleneck):
    def __init__(self):
        super(RAW, self).__init__("RAW")


class Virtual(Bottleneck):
    def __init__(self):
        super(Virtual, self).__init__("Virtual")


class BIdx(Enum):
    """
        `BIdx` specifies the bottleneck
        via accessing `bottleneck`.
    """
    Base = 0
    IcacheMiss = 1
    DcacheMiss = 2
    BPMiss = 3
    ROB = 4
    LQ = 5
    SQ = 6
    IntRF = 7
    FpRF = 8
    IQ = 9
    IntAlu = 10
    IntMultDiv = 11
    FpAlu = 12
    FpMultDiv = 13
    RdWrPort = 14
    RAW = 15
    Virtual = 16


bottleneck = []


def register_bottleneck(btk_cls: object):
    bottleneck.append(btk_cls().name)


register_bottleneck(Base)
register_bottleneck(IcacheMiss)
register_bottleneck(DcacheMiss)
register_bottleneck(BPMiss)
register_bottleneck(ROB)
register_bottleneck(LQ)
register_bottleneck(SQ)
register_bottleneck(IntRF)
register_bottleneck(FpRF)
register_bottleneck(IQ)
register_bottleneck(IntAlu)
register_bottleneck(IntMultDiv)
register_bottleneck(FpAlu)
register_bottleneck(FpMultDiv)
register_bottleneck(RdWrPort)
register_bottleneck(RAW)
register_bottleneck(Virtual)
