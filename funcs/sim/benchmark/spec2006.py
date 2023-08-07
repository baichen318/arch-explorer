# Author: baichen.bai@alibaba-inc.com
# SPEC2006 benchmarks macros


import os
from utils.utils import if_exist
from collections import OrderedDict
from funcs.sim.benchmark.benchmark import Benchmark


class SPEC2006(Benchmark):

    # support benchmarks
    _benchmark_suite = [
        # integer benchmarks
        "401.bzip2",
        "429.mcf",
        "456.hmmer",
        "458.sjeng",
        "462.libquantum",
        "464.h264ref",
        "471.omnetpp",
        "483.xalancbmk",
        # floating-point benchmarks
        "444.namd",
        "447.dealII",
        "450.soplex",
        "453.povray",
    ]

    _int_root = "spec2006_int_ref_public"
    _fp_root = "spec2006_fp_ref_public"

    def __init__(self, benchmark: dict):
        super(SPEC2006, self).__init__("spec2006")
        self._spec2006_root = benchmark["benchmark-root"]
        self._candidate = benchmark["candidate"]
        self._dse = benchmark["dse"]
        """
            Each benchmark has six entries, including
            1. benchmark-root: benchmark root directory,
            2. elf: benchmark executable binary path,
            3. options: benchmark input arguments,
            4. warmup-insts: warmup instructions.
            5. fast-forward: fast forward instructions.
            6. maxinsts: maximal instructions.
        """
        self._macros = self.construct_spec2006()

    @property
    def spec2006_root(self):
        return self._spec2006_root

    @property
    def candidate(self):
        return self._candidate

    @property
    def dse(self):
        return self._dse

    @property
    def macros(self):
        return self._macros

    @property
    def benchmark_suite(self):
        return SPEC2006._benchmark_suite

    def benchmark_root(self, name, fp: bool = True):
        if not fp:
            return os.path.join(
                self.spec2006_root,
                SPEC2006._int_root,
                name
            )
        else:
            return os.path.join(
                self.spec2006_root,
                SPEC2006._fp_root,
                name
            )

    def construct_spec2006(self):
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
        if "bzip2" in name:
            self.bzip2_macros(macros)
        elif "mcf" in name:
            self.mcf_macros(macros)
        elif "hmmer" in name:
            self.hmmer_macros(macros)
        elif "sjeng" in name:
            self.sjeng_macros(macros)
        elif "libquantum" in name:
            self.libquantum_macros(macros)
        elif "h264ref" in name:
            self.h264ref_macros(macros)
        elif "omnetpp" in name:
            self.omnetpp_macros(macros)
        elif "xalancbmk" in name:
            self.xalancbmk_macros(macros)
        elif "namd" in name:
            self.namd_macros(macros)
        elif "dealII" in name:
            self.dealII_macros(macros)
        elif "soplex" in name:
            self.soplex_macros(macros)
        elif "povray" in name:
            self.povray_macros(macros)

    def create_complete_benchmark_macros(self, macros):
        self.bzip2_macros(macros)
        self.mcf_macros(macros)
        self.hmmer_macros(macros)
        self.sjeng_macros(macros)
        self.libquantum_macros(macros)
        self.h264ref_macros(macros)
        self.omnetpp_macros(macros)
        self.xalancbmk_macros(macros)
        self.namd_macros(macros)
        self.dealII_macros(macros)
        self.soplex_macros(macros)
        self.povray_macros(macros)

    def bzip2_macros(self, macros):
        name = "401.bzip2"
        root = os.path.join(
            self.benchmark_root(name, fp=False)
        )
        num_of_insts = 10000000
        if self.dse:
            max_insts = 30000
        else:
            max_insts = 50000000
        macros[name] = {
            "benchmark-root": root,
            "elf": os.path.join(root, "{}.riscv".format(name)),
            "options": "{}".format(
                os.path.join(root, "input.source")
            ),
            "warmup-insts": num_of_insts,
            "fast-forward": num_of_insts,
            "maxinsts": max_insts
        }

    def mcf_macros(self, macros):
        name = "429.mcf"
        root = os.path.join(
            self.benchmark_root(name, fp=False)
        )
        num_of_insts = 10000000
        if self.dse:
            max_insts = 50000
        else:
            max_insts = 100000000
        macros[name] = {
            "benchmark-root": root,
            "elf": os.path.join(root, "{}.riscv".format(name)),
            "options": "{} 280".format(
                os.path.join(root, "inp.in")
            ),
            "warmup-insts": num_of_insts,
            "fast-forward": num_of_insts,
            "maxinsts": max_insts
        }

    def hmmer_macros(self, macros):
        name = "456.hmmer"
        root = os.path.join(
            self.benchmark_root(name, fp=False)
        )
        num_of_insts = 10000000
        if self.dse:
            max_insts = 50000
        else:
            max_insts = 100000000
        macros[name] = {
            "benchmark-root": root,
            "elf": os.path.join(root, "{}.riscv".format(name)),
            "options": "{} {}".format(
                os.path.join(root, "nph3.hmm"),
                os.path.join(root, "swiss41")
            ),
            "warmup-insts": num_of_insts,
            "fast-forward": num_of_insts,
            "maxinsts": max_insts
        }

    def sjeng_macros(self, macros):
        name = "458.sjeng"
        root = os.path.join(
            self.benchmark_root(name, fp=False)
        )
        num_of_insts = 10000000
        if self.dse:
            max_insts = 50000
        else:
            max_insts = 100000000
        macros[name] = {
            "benchmark-root": root,
            "elf": os.path.join(root, "{}.riscv".format(name)),
            "options": "{}".format(
                os.path.join(root, "ref.txt")
            ),
            "warmup-insts": num_of_insts,
            "fast-forward": num_of_insts,
            "maxinsts": max_insts
        }

    def libquantum_macros(self, macros):
        name = "462.libquantum"
        root = os.path.join(
            self.benchmark_root(name, fp=False)
        )
        num_of_insts = 10000000
        if self.dse:
            max_insts = 50000
        else:
            max_insts = 100000000
        macros[name] = {
            "benchmark-root": root,
            "elf": os.path.join(root, "{}.riscv".format(name)),
            "options": "1397 8",
            "warmup-insts": num_of_insts,
            "fast-forward": num_of_insts,
            "maxinsts": max_insts
        }

    def h264ref_macros(self, macros):
        name = "464.h264ref"
        root = os.path.join(
            self.benchmark_root(name, fp=False)
        )
        num_of_insts = 10000000
        if self.dse:
            max_insts = 50000
        else:
            max_insts = 100000000
        macros[name] = {
            "benchmark-root": root,
            "elf": os.path.join(root, "{}.riscv".format(name)),
            "options": "-d {}".format(
                os.path.join(
                    root, "foreman_ref_encoder_baseline.cfg"
                )
            ),
            "warmup-insts": num_of_insts,
            "fast-forward": num_of_insts,
            "maxinsts": max_insts
        }

    def omnetpp_macros(self, macros):
        name = "471.omnetpp"
        root = os.path.join(
            self.benchmark_root(name, fp=False)
        )
        num_of_insts = 10000000
        if self.dse:
            max_insts = 50000
        else:
            max_insts = 100000000
        macros[name] = {
            "benchmark-root": root,
            "elf": os.path.join(root, "{}.riscv".format(name)),
            "options": "-d {}".format(
                os.path.join(root, "omnetpp.ini")
            ),
            "warmup-insts": num_of_insts,
            "fast-forward": num_of_insts,
            "maxinsts": max_insts
        }

    def xalancbmk_macros(self, macros):
        name = "483.xalancbmk"
        root = os.path.join(
            self.benchmark_root(name, fp=False)
        )
        num_of_insts = 10000000
        if self.dse:
            max_insts = 50000
        else:
            max_insts = 100000000
        macros[name] = {
            "benchmark-root": root,
            "elf": os.path.join(root, "{}.riscv".format(name)),
            "options": "-v {} {}".format(
                os.path.join(root, "t5.xml"),
                os.path.join(root, "xalanc.xsl")
            ),
            "warmup-insts": num_of_insts,
            "fast-forward": num_of_insts,
            "maxinsts": max_insts
        }

    def namd_macros(self, macros):
        name = "444.namd"
        root = os.path.join(
            self.benchmark_root(name)
        )
        num_of_insts = 10000000
        if self.dse:
            max_insts = 50000
        else:
            max_insts = 100000000
        macros[name] = {
            "benchmark-root": root,
            "elf": os.path.join(root, "{}.riscv".format(name)),
            "options": "--input {} --iterations 38".format(
                os.path.join(root, "namd.input")
            ),
            "warmup-insts": num_of_insts,
            "fast-forward": num_of_insts,
            "maxinsts": max_insts
        }

    def dealII_macros(self, macros):
        name = "447.dealII"
        root = os.path.join(
            self.benchmark_root(name)
        )
        num_of_insts = 10000000
        if self.dse:
            max_insts = 50000
        else:
            max_insts = 100000000
        macros[name] = {
            "benchmark-root": root,
            "elf": os.path.join(root, "{}.riscv".format(name)),
            "options": "23",
            "warmup-insts": num_of_insts,
            "fast-forward": num_of_insts,
            "maxinsts": max_insts
        }

    def soplex_macros(self, macros):
        name = "450.soplex"
        root = os.path.join(
            self.benchmark_root(name)
        )
        num_of_insts = 10000000
        if self.dse:
            max_insts = 50000
        else:
            max_insts = 100000000
        macros[name] = {
            "benchmark-root": root,
            "elf": os.path.join(root, "{}.riscv".format(name)),
            "options": "-m3500 {}".format(
                os.path.join(root, "ref.mps")
            ),
            "warmup-insts": num_of_insts,
            "fast-forward": num_of_insts,
            "maxinsts": max_insts
        }

    def povray_macros(self, macros):
        name = "453.povray"
        root = os.path.join(
            self.benchmark_root(name)
        )
        num_of_insts = 10000000
        if self.dse:
            max_insts = 50000
        else:
            max_insts = 100000000
        macros[name] = {
            "benchmark-root": root,
            "elf": os.path.join(root, "{}.riscv".format(name)),
            "options": "{}".format(
                os.path.join(root, "SPEC-benchmark-ref.ini")
            ),
            "warmup-insts": num_of_insts,
            "fast-forward": num_of_insts,
            "maxinsts": max_insts
        }

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


def construct_spec2006(benchmark: dict):
    return SPEC2006(benchmark)
