# Author: baichen.bai@alibaba-inc.com
# SPEC2017 benchmarks macros


import os
from collections import OrderedDict
from utils.utils import assert_error
from funcs.sim.benchmark.benchmark import Benchmark


class SPEC2017(Benchmark):

    # support benchmarks
    _benchmark_suite = [
        # integer benchmarks
        "600.perlbench_s",
        "602.gcc_s",
        "605.mcf_s",
        "623.xalancbmk_s",
        "625.x264_s",
        "631.deepsjeng_s",
        "641.leela_s",
        "657.xz_s",
        # floating-point benchmarks
        # "603.bwaves_s", # this benchmark is too slow!
        "607.cactuBSSN_s",
        "619.lbm_s",
        "621.wrf_s",
        "627.cam4_s",
        "628.pop2_s",
        "638.imagick_s",
        "644.nab_s"
    ]

    _prefix = "run/run_base_refspeed_rv64g-gcc-9.2-64.0000"
    _suffix = "rv64g-gcc-9.2-64"

    def __init__(self, benchmark: dict):
        super(SPEC2017, self).__init__("spec2017")
        self._spec2017_root = benchmark["benchmark-root"]
        self._checkpoint_root = benchmark["checkpoint-root"]
        self._candidate = benchmark["candidate"]
        self._max_insts = benchmark["max-insts"]
        """
            Each benchmark has six entries, including
            1. benchmark-root: benchmark root directory,
            2. elf: benchmark executable binary path,
            3. options: benchmark input arguments,
            4. inputs: benchmark input files' root path,
            5. checkpoint: the # of checkpoint,
            6. checkpoint-root: checkpoint root path.
        """
        self._macros = self.construct_spec2017()

    @property
    def spec2017_root(self):
        return self._spec2017_root

    @property
    def checkpoint_root(self):
        return self._checkpoint_root

    @property
    def candidate(self):
        return self._candidate

    @property
    def max_insts(self):
        return self._max_insts

    @property
    def benchmark_suite(self):
        return SPEC2017._benchmark_suite

    @property
    def prefix(self):
        return SPEC2017._prefix

    @property
    def suffix(self):
        return SPEC2017._suffix

    def bencmark_name(self, idx):
        assert idx < len(self.benchmark_suite), \
            assert_error(
                "{} is out of bounds. " \
                "total benchmark: {}".format(
                    idx, len(self.benchmark_suite)
                )
            )
        return self.benchmark_suite[idx]

    def alias(self, name):
        return name.split('.')[1]

    def benchmark_root(self, name):
        return os.path.join(
            self.spec2017_root,
            name,
            self.prefix
        )

    def construct_spec2017(self):
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
        if "perlbench" in name:
            self.perlbench_macros(macros)
        elif "gcc" in name:
            self.gcc_macros(macros)
        elif "mcf" in name:
            self.mcf_macros(macros)
        elif "xalancbmk" in name:
            self.xalancbmk_macros(macros)
        elif "x264" in name:
            self.x264_macros(macros)
        elif "deepsjeng" in name:
            self.deepsjeng_macros(macros)
        elif "leela" in name:
            self.leela_macros(macros)
        elif "xz" in name:
            self.xz_macros(macros)
        # elif "bwaves" in name:
        #     self.bwaves_macros(macros)
        elif "cactuBSSN" in name:
            self.cactubssn_macros(macros)
        elif "lbm" in name:
            self.lbm_macros(macros)
        elif "wrf" in name:
            self.wrf_macros(macros)
        elif "cam4" in name:
            self.cam4_macros(macros)
        elif "pop2" in name:
            self.pop2_macros(macros)
        elif "imagick" in name:
            self.imagick_macros(macros)
        elif "nab" in name:
            self.nab_macros(macros)

    def create_complete_benchmark_macros(self, macros):
        # integer benchmarks
        self.perlbench_macros(macros)
        self.gcc_macros(macros)
        self.mcf_macros(macros)
        # self.omnetpp_macros(macros)
        self.xalancbmk_macros(macros)
        self.x264_macros(macros)
        self.deepsjeng_macros(macros)
        self.leela_macros(macros)
        # self.exchange2_macros(macros)
        self.xz_macros(macros)
        # floating-point benchmarks
        # self.bwaves_macros(macros)
        self.cactubssn_macros(macros)
        # self.namd_macros(macros)
        # self.parest_macros(macros)
        # self.povray_macros(macros)
        self.lbm_macros(macros)
        self.wrf_macros(macros)
        # self.blender_macros(macros)
        self.cam4_macros(macros)
        self.pop2_macros(macros)
        self.imagick_macros(macros)
        self.nab_macros(macros)
        # self.fotonik3d_macros(macros)
        # self.roms_macros(macros)

    def perlbench_macros(self, macros):
        macros["600.perlbench_s"] = {
            "benchmark-root": self.benchmark_root("600.perlbench_s"),
            "elf": os.path.join(
                self.benchmark_root("600.perlbench_s"),
                "{}_base.{}".format(
                    self.alias("600.perlbench_s"),
                    self.suffix
                )
            ),
            "options": "-I{}/lib " \
                "{} " \
                "2500 5 25 11 150 1 1 1 1".format(
                    self.benchmark_root("600.perlbench_s"),
                    os.path.join(
                        self.benchmark_root("600.perlbench_s"),
                        "checkspam.pl"
                    )
                ),
            "inputs": None,
            "checkpoint": 14,
            "checkpoint-root": os.path.join(
                self.checkpoint_root,
                "600.perlbench_s",
                "{}-checkpoint".format("600.perlbench_s")
            )
        }

    def gcc_macros(self, macros):
        macros["602.gcc_s"] = {
            "benchmark-root": self.benchmark_root("602.gcc_s"),
            "elf": os.path.join(
                self.benchmark_root("602.gcc_s"),
                "sgcc_base.{}".format(
                    self.suffix
                )
            ),
            "options": "{}/gcc-pp.c " \
                "-O5 " \
                "-fipa-pta " \
                "-o " \
                "gcc-pp.opts-O5_-fipa-pta.s".format(
                    self.benchmark_root("602.gcc_s")
                ),
            "inputs": None,
            "checkpoint": 8,
            "checkpoint-root": os.path.join(
                self.checkpoint_root,
                "602.gcc_s",
                "{}-checkpoint".format("602.gcc_s")
            )
        }

    def mcf_macros(self, macros):
        macros["605.mcf_s"] = {
            "benchmark-root": self.benchmark_root("605.mcf_s"),
            "elf": os.path.join(
                self.benchmark_root("605.mcf_s"),
                "{}_base.{}".format(
                    self.alias("605.mcf_s"),
                    self.suffix
                )
            ),
            "options": os.path.join(
                self.benchmark_root("605.mcf_s"),
                "inp.in"
            ),
            "inputs": None,
            "checkpoint": 7,
            "checkpoint-root": os.path.join(
                self.checkpoint_root,
                "605.mcf_s",
                "{}-checkpoint".format("605.mcf_s")
            )
        }

    def xalancbmk_macros(self, macros):
        macros["623.xalancbmk_s"] = {
            "benchmark-root": self.benchmark_root("623.xalancbmk_s"),
            "elf": os.path.join(
                self.benchmark_root("623.xalancbmk_s"),
                "{}_base.{}".format(
                    self.alias("623.xalancbmk_s"),
                    self.suffix
                )
            ),
            "options": "-v {}/t5.xml {}/xalanc.xsl".format(
                self.benchmark_root("623.xalancbmk_s"),
                self.benchmark_root("623.xalancbmk_s")
            ),
            "inputs": None,
            "checkpoint": 7,
            "checkpoint-root": os.path.join(
                self.checkpoint_root,
                "623.xalancbmk_s",
                "{}-checkpoint".format("623.xalancbmk_s")
            )
        }

    def x264_macros(self, macros):
        macros["625.x264_s"] = {
            "benchmark-root": self.benchmark_root("625.x264_s"),
            "elf": os.path.join(
                self.benchmark_root("625.x264_s"),
                "{}_base.{}".format(
                    self.alias("625.x264_s"),
                    self.suffix
                )
            ),
            "options": "--pass 1 --stats x264_stats.log " \
                "--bitrate 1000 --frames 1000 -o 'BuckBunny_New.264 " \
                "{}/BuckBunny.yuv 1280x720".format(
                    os.path.join(
                        self.benchmark_root("625.x264_s")
                    )
                ),
            "inputs": None,
            "checkpoint": 4,
            "checkpoint-root": os.path.join(
                self.checkpoint_root,
                "625.x264_s",
                "{}-checkpoint".format("625.x264_s")
            )
        }

    def deepsjeng_macros(self, macros):
        macros["631.deepsjeng_s"] = {
            "benchmark-root": self.benchmark_root("631.deepsjeng_s"),
            "elf": os.path.join(
                self.benchmark_root("631.deepsjeng_s"),
                "{}_base.{}".format(
                    self.alias("631.deepsjeng_s"),
                    self.suffix
                )
            ),
            "options": os.path.join(
                self.benchmark_root("631.deepsjeng_s"),
                "ref.txt"
            ),
            "inputs": None,
            "checkpoint": 2,
            "checkpoint-root": os.path.join(
                self.checkpoint_root,
                "631.deepsjeng_s",
                "{}-checkpoint".format("631.deepsjeng_s")
            )
        }

    def leela_macros(self, macros):
        macros["641.leela_s"] = {
            "benchmark-root": self.benchmark_root("641.leela_s"),
            "elf": os.path.join(
                self.benchmark_root("641.leela_s"),
                "{}_base.{}".format(
                    self.alias("641.leela_s"),
                    self.suffix
                )
            ),
            "options": os.path.join(
                self.benchmark_root("641.leela_s"),
                "ref.sgf"
            ),
            "inputs": None,
            "checkpoint": 9,
            "checkpoint-root": os.path.join(
                self.checkpoint_root,
                "641.leela_s",
                "{}-checkpoint".format("641.leela_s")
            )
        }

    def xz_macros(self, macros):
        macros["657.xz_s"] = {
            "benchmark-root": self.benchmark_root("657.xz_s"),
            "elf": os.path.join(
                self.benchmark_root("657.xz_s"),
                "{}_base.{}".format(
                    self.alias("657.xz_s"),
                    self.suffix
                )
            ),
            "options": "{}/cpu2006docs.tar.xz " \
                "6643 " \
                "055ce243071129412e9dd0b3b69a21654033a9b723d874b20" \
                "15c774fac1553d9713be561ca86f74e4f16f22e664fc17a79" \
                "f30caa5ad2c04fbc447549c2810fae " \
                "1036078272 " \
                "1111795472 " \
                "4".format(
                    self.benchmark_root("657.xz_s")
                ),
            "inputs": None,
            "checkpoint": 11,
            "checkpoint-root": os.path.join(
                self.checkpoint_root,
                "657.xz_s",
                "{}-checkpoint".format("657.xz_s")
            )
        }

    def bwaves_macros(self, macros):
        macros["603.bwaves_s"] = {
            "benchmark-root": self.benchmark_root("603.bwaves_s"),
            "elf": os.path.join(
                self.benchmark_root("603.bwaves_s"),
                "speed_bwaves_base.{}".format(
                    self.suffix
                )
            ),
            "options": "bwaves_1",
            "inputs": os.path.join(
                self.benchmark_root("603.bwaves_s"),
                "bwaves_1.in"
            ),
            "checkpoint": 1,
            "checkpoint-root": os.path.join(
                self.checkpoint_root,
                "603.bwaves_s",
                "{}-checkpoint".format("603.bwaves_s")
            )
        }

    def cactubssn_macros(self, macros):
        macros["607.cactuBSSN_s"] = {
            "benchmark-root": self.benchmark_root("607.cactuBSSN_s"),
            "elf": os.path.join(
                self.benchmark_root("607.cactuBSSN_s"),
                "{}_base.{}".format(
                    self.alias("607.cactuBSSN_s"),
                    self.suffix
                )
            ),
            "options": os.path.join(
                self.benchmark_root("607.cactuBSSN_s"),
                "spec_ref.par"
            ),
            "inputs": None,
            "checkpoint": 3,
            "checkpoint-root": os.path.join(
                self.checkpoint_root,
                "607.cactuBSSN_s",
                "{}-checkpoint".format("607.cactuBSSN_s")
            )
        }

    def lbm_macros(self, macros):
        macros["619.lbm_s"] = {
            "benchmark-root": self.benchmark_root("619.lbm_s"),
            "elf": os.path.join(
                self.benchmark_root("619.lbm_s"),
                "{}_base.{}".format(
                    self.alias("619.lbm_s"),
                    self.suffix
                )
            ),
            "options": "2000 reference.dat 0 0 " \
                "{}/200_200_260_ldc.of".format(
                    self.benchmark_root("619.lbm_s")
                ),
            "inputs": None,
            "checkpoint": 1,
            "checkpoint-root": os.path.join(
                self.checkpoint_root,
                "619.lbm_s",
                "{}-checkpoint".format("619.lbm_s")
            )
        }

    def wrf_macros(self, macros):
        macros["621.wrf_s"] = {
            "benchmark-root": self.benchmark_root("621.wrf_s"),
            "elf": os.path.join(
                self.benchmark_root("621.wrf_s"),
                "{}_base.{}".format(
                    self.alias("621.wrf_s"),
                    self.suffix
                )
            ),
            "options": None,
            "inputs": None,
            "checkpoint": 7,
            "checkpoint-root": os.path.join(
                self.checkpoint_root,
                "621.wrf_s",
                "{}-checkpoint".format("621.wrf_s")
            )
        }

    def cam4_macros(self, macros):
        macros["627.cam4_s"] = {
            "benchmark-root": self.benchmark_root("627.cam4_s"),
            "elf": os.path.join(
                self.benchmark_root("627.cam4_s"),
                "{}_base.{}".format(
                    self.alias("627.cam4_s"),
                    self.suffix
                )
            ),
            "options": None,
            "inputs": None,
            "checkpoint": 10,
            "checkpoint-root": os.path.join(
                self.checkpoint_root,
                "627.cam4_s",
                "{}-checkpoint".format("627.cam4_s")
            )
        }

    def pop2_macros(self, macros):
        macros["628.pop2_s"] = {
            "benchmark-root": self.benchmark_root("628.pop2_s"),
            "elf": os.path.join(
                self.benchmark_root("628.pop2_s"),
                "speed_pop2_base.{}".format(
                    self.suffix
                )
            ),
            "options": None,
            "inputs": None,
            "checkpoint": 5,
            "checkpoint-root": os.path.join(
                self.checkpoint_root,
                "628.pop2_s",
                "{}-checkpoint".format("628.pop2_s")
            )
        }

    def imagick_macros(self, macros):
        macros["638.imagick_s"] = {
            "benchmark-root": self.benchmark_root("638.imagick_s"),
            "elf": os.path.join(
                self.benchmark_root("638.imagick_s"),
                "{}_base.{}".format(
                    self.alias("638.imagick_s"),
                    self.suffix
                )
            ),
            "options": "-limit disk 0 {}/refspeed_input.tga " \
                "-resize 817% " \
                "-rotate -2.76 -shave 540x375 -alpha remove " \
                "-auto-level -contrast-stretch 1x1% " \
                "-colorspace Lab -channel R -equalize " \
                "+channel -colorspace sRGB -define " \
                "histogram:unique-colors=false -adaptiveblur" \
                "0x5 -despeckle -auto-gamma -adaptive-sharpen " \
                "55 -enhance -brightness-contrast 10x10 -resize " \
                "30% refspeed_output.tga".format(
                    self.benchmark_root("638.imagick_s")
                ),
            "inputs": None,
            "checkpoint": 3,
            "checkpoint-root": os.path.join(
                self.checkpoint_root,
                "638.imagick_s",
                "{}-checkpoint".format("638.imagick_s")
            )
        }

    def nab_macros(self, macros):
        macros["644.nab_s"] = {
            "benchmark-root": self.benchmark_root("644.nab_s"),
            "elf": os.path.join(
                self.benchmark_root("644.nab_s"),
                "{}_base.{}".format(
                    self.alias("644.nab_s"),
                    self.suffix
                )
            ),
            "options": "3j1n 20140317 220",
            "inputs": None,
            "checkpoint": 1,
            "checkpoint-root": os.path.join(
                self.checkpoint_root,
                "644.nab_s",
                "{}-checkpoint".format("644.nab_s")
            )
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


def construct_spec2017(benchmark: dict):
    return SPEC2017(benchmark)
