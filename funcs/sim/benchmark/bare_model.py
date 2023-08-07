# Author: baichen.bai@alibaba-inc.com


import os
from utils.utils import if_exist
from collections import OrderedDict
from funcs.sim.benchmark.benchmark import Benchmark


class BareModel(Benchmark):

    # support benchmarks
    _benchmark_suite = [
        "dhrystone.riscv",
        "median.riscv",
        "multiply.riscv",
        "qsort.riscv",
        "rsort.riscv",
        "spmv.riscv",
        "towers.riscv",
        "vvadd.riscv"
    ]

    def __init__(self, benchmark: dict):
        super(BareModel, self).__init__("bare-model")
        self._bare_model_root = os.path.abspath(
            os.path.join(
                os.path.dirname(__file__),
                os.path.pardir,
                os.path.pardir,
                os.path.pardir,
                "infras",
                "gem5-research",
                "benchmarks",
                "riscv-tests"
            )
        )
        self._candidate = benchmark["candidate"]
        self._warmup_insts = benchmark["warmup-insts"]
        self._fast_forward = benchmark["fast-forward"]
        """
            Each benchmark has six entries, including
            1. benchmark-root: benchmark root directory,
            2. elf: benchmark executable binary path,
            3. options: benchmark input arguments,
            4. inputs: benchmark input files' root path.
        """
        self._macros = self.construct_bare_model()
        self.validate()

    @property
    def bare_model_root(self):
        return self._bare_model_root

    @property
    def candidate(self):
        return self._candidate

    @property
    def warmup_insts(self):
        return self._warmup_insts

    @property
    def fast_forward(self):
        return self._fast_forward

    @property
    def benchmark_suite(self):
        return BareModel._benchmark_suite

    def benchmark_root(self, name):
        return os.path.join(
            self.bare_model_root,
            name
        )

    def construct_bare_model(self):
        macros = OrderedDict()
        if self.candidate is not None:
            for name in self.candidate:
                self.create_single_benchmark_macros(
                    macros, name
                )
        else:
            self.create_complete_benchmark_macros(macros)
        return macros

    def create_single_benchmark_macros(self, macros, name):
        if "dhrystone" in name:
            self.dhrystone_macros(macros)
        elif "median" in name:
            self.median_macros(macros)
        elif "multiply" in name:
            self.multiply_macros(macros)
        elif "qsort" in name:
            self.qsort_macros(macros)
        elif "rsort" in name:
            self.rsort_macros(macros)
        elif "spmv" in name:
            self.spmv_macros(macros)
        elif "towers" in name:
            self.towers_macros(macros)
        elif "vvadd" in name:
            self.vvadd_macros(macros)

    def create_complete_benchmark_macros(self, macros):
        self.dhrystone_macros(macros)
        self.median_macros(macros)
        self.multiply_macros(macros)
        self.qsort_macros(macros)
        self.rsort_macros(macros)
        self.spmv_macros(macros)
        self.towers_macros(macros)
        self.vvadd_macros(macros)

    def dhrystone_macros(self, macros):
        macros["dhrystone"] = {
            "benchmark-root": self.bare_model_root,
            "elf": os.path.join(
                self.benchmark_root("dhrystone.riscv")
            ),
            "options": None,
            "inputs": None
        }

    def median_macros(self, macros):
        macros["median"] = {
            "benchmark-root": self.bare_model_root,
            "elf": os.path.join(
                self.benchmark_root("median.riscv")
            ),
            "options": None,
            "inputs": None
        }

    def multiply_macros(self, macros):
        macros["multiply"] = {
            "benchmark-root": self.bare_model_root,
            "elf": os.path.join(
                self.benchmark_root("multiply.riscv")
            ),
            "options": None,
            "inputs": None
        }

    def qsort_macros(self, macros):
        macros["qsort"] = {
            "benchmark-root": self.bare_model_root,
            "elf": os.path.join(
                self.benchmark_root("qsort.riscv")
            ),
            "options": None,
            "inputs": None
        }

    def rsort_macros(self, macros):
        macros["rsort"] = {
            "benchmark-root": self.bare_model_root,
            "elf": os.path.join(
                self.benchmark_root("rsort.riscv")
            ),
            "options": None,
            "inputs": None
        }

    def spmv_macros(self, macros):
        macros["spmv"] = {
            "benchmark-root": self.bare_model_root,
            "elf": os.path.join(
                self.benchmark_root("spmv.riscv")
            ),
            "options": None,
            "inputs": None
        }

    def towers_macros(self, macros):
        macros["towers"] = {
            "benchmark-root": self.bare_model_root,
            "elf": os.path.join(
                self.benchmark_root("towers.riscv")
            ),
            "options": None,
            "inputs": None
        }

    def vvadd_macros(self, macros):
        macros["vvadd"] = {
            "benchmark-root": self.bare_model_root,
            "elf": os.path.join(
                self.benchmark_root("vvadd.riscv")
            ),
            "options": None,
            "inputs": None
        }

    def validate(self):
        for k, v in self.macros.items():
            if_exist(v["elf"], strict=True, quiet=False)

    def __len__(self):
        return len(self.macros)

    def __iter__(self):
        for k, v in self.macros.items():
            yield k, v

    def __getitem__(self, item):
        try:
            return self.macros[item]
        except KeyError:
            raise KeyError(
                "item: {} is not found.".format(idx)
            )


def construct_bare_model(benchmark: dict):
    return BareModel(benchmark)
