# Author: baichen.bai@alibaba-inc.com


import os
import abc
import platform
from typing import Optional, Dict


class Simulation(abc.ABC):
    def __init__(self, configs: Dict):
        super(Simulation, self).__init__()
        self.configs = configs
        self.macros = {}
        self.macros["arch-explorer-root"] = os.path.abspath(
            os.path.join(
                os.path.dirname(__file__),
                os.pardir,
                os.pardir
            )
        )
        self.macros["infras-root"] = os.path.join(
            self.macros["arch-explorer-root"],
            "infras"
        )
        self.macros["gem5-research-root"] = \
            self.configs["simulator"]["gem5-research-root"]
        self.macros["mcpat-research-root"] = os.path.abspath(
            os.path.join(
                self.macros["infras-root"],
                "mcpat-research"
            )
        )
        self.macros["pydeg-root"] = os.path.abspath(
            os.path.join(
                self.macros["arch-explorer-root"],
                "algo",
                "core"
            )
        )
        self.macros["cppdeg-root"] = os.path.abspath(
            os.path.join(
                self.macros["arch-explorer-root"],
                "infras",
                "deg",
                "build"
            )
        )
        self.contruct_gem5_macros()

    def contruct_gem5_macros(self):
        self.macros["base-o3-cpu"] = os.path.abspath(
            os.path.join(
                self.macros["gem5-research-root"],
                "src",
                "cpu",
                "o3",
                "BaseO3CPU.py"
            )
        )
        self.macros["branch-predictor"] = os.path.abspath(
            os.path.join(
                self.macros["gem5-research-root"],
                "src",
                "cpu",
                "pred",
                "BranchPredictor.py"
            )
        )
        self.macros["fu-pool"] = os.path.abspath(
            os.path.join(
                self.macros["gem5-research-root"],
                "src",
                "cpu",
                "o3",
                "FUPool.py"
            )
        )
        self.macros["riscv-tlb"] = os.path.abspath(
            os.path.join(
                self.macros["gem5-research-root"],
                "src",
                "arch",
                "riscv",
                "RiscvTLB.py"
            )
        )
        self.macros["build-root"] = os.path.abspath(
            os.path.join(
                self.macros["gem5-research-root"],
                "build",
                "RISCV"
            )
        )
        self.macros["temp-root"] = os.path.abspath(
            os.path.join(
                self.macros["arch-explorer-root"],
                "temp"
            )
        )
        self.macros["m5out-root"] = os.path.abspath(
            os.path.join(
                self.macros["gem5-research-root"],
                "m5out"
            )
        )

    @abc.abstractmethod
    def simulate(self, embedding: list):
        raise NotImplementedError()
