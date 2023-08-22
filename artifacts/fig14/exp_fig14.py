# Author: baichen.bai@alibaba-inc.com


import os
import re
import sys
import argparse
import numpy as np
import matplotlib.pyplot as plt
from collections import OrderedDict
from funcs.sim.o3cpu.o3cpu_simulation import O3CPUSimulation
from funcs.design.o3cpu.o3cpu_design_space import parse_design_space
from utils.utils import get_configs_from_command, execute, \
    remove, mkdir, info, error, if_exist, copy, assert_error, get_configs


SPEC06_CONST = OrderedDict({
    "ADABOOST": 581298471162024,
    "ARCHRANKER": 433456751235984,
    "BOOM_EXPLORER": 588950488395454,
    "ARCH_EXPLORER": 653634162090455
})


SPEC17_CONST =OrderedDict({
    "ADABOOST": 204919127918344,
    "ARCHRANKER": 718636279781471,
    "BOOM_EXPLORER": 588894640080383,
    "ARCH_EXPLORER": 885449625238848
})


SPEC06_PARTIAL_BENCHMARK = OrderedDict({
    # integer benchmarks
    "401.bzip2": "BZIP2",
    "429.mcf": "MCF",
    "456.hmmer": "HMMER",
    "458.sjeng": "SJENG",
    "462.libquantum": "LIBQUANTUM",
    "471.omnetpp": "OMNETPP",
    "483.xalancbmk": "XALANCBMK",
    # floating-point benchmarks
    "444.namd": "NAMD",
    "447.dealII": "DEALII",
    "450.soplex": "SOPLEX",
    "453.povray": "POVRAY"
})


SPEC06_FULL_BENCHMARK = OrderedDict({
    # integer benchmarks
    "401.bzip2": "BZIP2",
    "429.mcf": "MCF",
    "456.hmmer": "HMMER",
    "458.sjeng": "SJENG",
    "462.libquantum": "LIBQUANTUM",
    "464.h264ref": "H264REF",
    "471.omnetpp": "OMNETPP",
    "483.xalancbmk": "XALANCBMK",
    # floating-point benchmarks
    "444.namd": "NAMD",
    "447.dealII": "DEALII",
    "450.soplex": "SOPLEX",
    "453.povray": "POVRAY"
})


SPEC17_PARTIAL_BENCHMARK = OrderedDict({
    # integer benchmarks
    "602.gcc_s": "GCC",
    "605.mcf_s": "MCF",
    "631.deepsjeng_s": "DEEPSJENG",
    "641.leela_s": "LEELA",
    "657.xz_s": "XZ",
    # floating-point benchmarks
    # "603.bwaves_s", # this benchmark is too slow!
    "607.cactuBSSN_s": "CACTUBSSN",
    "619.lbm_s": "LBM",
    "621.wrf_s": "WRF",
    "627.cam4_s": "CAM4",
    "628.pop2_s": "POP2",
    "644.nab_s": "NAB"
})


SPEC17_FULL_BENCHMARK = OrderedDict({
    # integer benchmarks
    "600.perlbench_s": "PERLBENCH",
    "602.gcc_s": "GCC",
    "605.mcf_s": "MCF",
    "625.x264_s": "X264",
    "631.deepsjeng_s": "DEEPSJENG",
    "641.leela_s": "LEELA",
    "657.xz_s": "XZ",
    # floating-point benchmarks
    # "603.bwaves_s", # this benchmark is too slow!
    "607.cactuBSSN_s": "CACTUBSSN",
    "619.lbm_s": "LBM",
    "621.wrf_s": "WRF",
    "627.cam4_s": "CAM4",
    "628.pop2_s": "POP2",
    "638.imagick_s": "IMAGICK",
    "644.nab_s": "NAB"
})


def parse_multi_args():
    def initialize_parser(parser):
        parser.add_argument(
            "-c", "--configs",
            required=True,
            type=str,
            default=[],
            action="append",
            help="YAML files to be handled")
        return parser

    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser = initialize_parser(parser)
    return parser.parse_args()


def simulate():
    # check benchmark path for SPEC06
    configs = get_configs(exp_fig14_spec06_yml)
    if not if_exist(
        configs["simulation"]["benchmark"]["spec2006"]["benchmark-root"],
        quiet=False
    ):
        error("SPEC06 benchmark root directory path is not found. "
            "Please have a double-check (could docker mapping be incorrect?).")

    # check benchmark path for SPEC17
    configs = get_configs(exp_fig14_spec17_yml)
    if not if_exist(
        configs["simulation"]["benchmark"]["spec2017"]["benchmark-root"],
        quiet=False
    ) or not if_exist(
        configs["simulation"]["benchmark"]["spec2017"]["checkpoint-root"],
        quiet=False
    ):
        error("SPEC17 benchmark root directory path is not found. "
            "Please have a double-check (could docker mapping be incorrect?).")

    # SPEC06
    cmd = "{} {} -c {}".format(
        sys.executable,
        os.path.join(archexplorer_root, "main", "main.py"),
        exp_fig14_spec06_yml
    )
    execute(cmd)

    # SPEC17
    cmd = "{} {} -c {}".format(
        sys.executable,
        os.path.join(archexplorer_root, "main", "main.py"),
        exp_fig14_spec17_yml
    )
    execute(cmd)


def get_sim_result():
    if "partial" in exp_fig14_spec06_yml:
        SPEC_06_BENCHMARK = SPEC06_PARTIAL_BENCHMARK
        SPEC_17_BENCHMARK = SPEC17_PARTIAL_BENCHMARK
    else:
        SPEC_06_BENCHMARK = SPEC06_FULL_BENCHMARK
        SPEC_17_BENCHMARK = SPEC17_FULL_BENCHMARK

    result_summary = OrderedDict()

    # SPEC06
    for idx in SPEC06_CONST.values():
        result_summary[idx] = OrderedDict()
        cmd = "{} {} -i {}".format(
            sys.executable,
            os.path.join(archexplorer_root, "tools", "get_ppa.py"),
            idx
        )
        execute(cmd)

        for benchmark, alias in SPEC_06_BENCHMARK.items():
            benchmark_root = os.path.join(
                archexplorer_root, "temp", "gem5-{}".format(idx), benchmark
            )
            _ppa_rpt = os.path.join(benchmark_root, "ppa.rpt")
            with open(_ppa_rpt, 'r') as f:
                content = f.readlines()[0]
                result = re.findall(pat_ppa_rpt, content)
                ipc = float(result[0])
                cpi = float(result[1])
                power = float(result[2])
                area = float(result[3])
                pppa = (ipc * ipc) / (power * area)
                result_summary[idx][alias] = [ipc, cpi, power, area, pppa]

    for idx in SPEC17_CONST.values():
        result_summary[idx] = OrderedDict()
        cmd = "{} {} -i {}".format(
            sys.executable,
            os.path.join(archexplorer_root, "tools", "get_ppa.py"),
            idx
        )
        execute(cmd)

        for benchmark, alias in SPEC_17_BENCHMARK.items():
            benchmark_root = os.path.join(
                archexplorer_root, "temp", "gem5-{}".format(idx), benchmark
            )
            _ppa_rpt = os.path.join(benchmark_root, "ppa.rpt")
            with open(_ppa_rpt, 'r') as f:
                content = f.readlines()[0]
                result = re.findall(pat_ppa_rpt, content)
                ipc = float(result[0])
                cpi = float(result[1])
                power = float(result[2])
                area = float(result[3])
                pppa = (ipc * ipc) / (power * area)
                result_summary[idx][alias] = [ipc, cpi, power, area, pppa]

    return result_summary


def plot(result_summary):
    # plot
    if "partial" in exp_fig14_spec06_yml:
        SPEC_06_BENCHMARK = SPEC06_PARTIAL_BENCHMARK
        SPEC_17_BENCHMARK = SPEC17_PARTIAL_BENCHMARK
        tex = "fig14_partial_mode_template.tex"
        name = "fig14_partial_mode"
    else:
        SPEC_06_BENCHMARK = SPEC06_FULL_BENCHMARK
        SPEC_17_BENCHMARK = SPEC17_FULL_BENCHMARK
        tex = "fig14_full_mode_template.tex"
        name = "fig14_full_mode"

    props = [
        "PERF", "POWER", "AREA", "PPPA"
    ]

    with open(os.path.join(cur_root, tex), 'r') as f:
        data = f.read()

        for method, idx in SPEC06_CONST.items():
            for benchmark, alias in SPEC_06_BENCHMARK.items():
                for prop in props:
                    value = method + "_SPEC06_" + alias + '_' + prop
                    if prop == "PERF":
                        data = data.replace(value, str(result_summary[idx][alias][0]))
                    elif prop == "POWER":
                        data = data.replace(value, str(result_summary[idx][alias][2]))
                    elif prop == "AREA":
                        data = data.replace(value, str(result_summary[idx][alias][3]))
                    else:
                        assert prop == "PPPA", \
                            assert_error("unknown prop: {}".format(prop))
                        data = data.replace(value, str(result_summary[idx][alias][4]))

        for method, idx in SPEC06_CONST.items():
            avg_perf, avg_power, avg_area, avg_pppa = [], [], [], []
            for benchmark, alias in SPEC_06_BENCHMARK.items():
                avg_perf.append(result_summary[idx][alias][0])
                avg_power.append(result_summary[idx][alias][2])
                avg_area.append(result_summary[idx][alias][3])
                avg_pppa.append(result_summary[idx][alias][4])
            avg_perf = np.average(avg_perf)
            avg_power = np.average(avg_power)
            avg_area = np.average(avg_area)
            avg_pppa = np.average(avg_pppa)
            for prop in props:
                value = method + "_SPEC06_" + prop
                if prop == "PERF":
                    data = data.replace(value, str(avg_perf))
                elif prop == "POWER":
                    data = data.replace(value, str(avg_power))
                elif prop == "AREA":
                    data = data.replace(value, str(avg_area))
                else:
                    assert prop == "PPPA", \
                        assert_error("unknown prop: {}".format(prop))
                    data = data.replace(value, str(avg_pppa))

        for method, idx in SPEC17_CONST.items():
            for benchmark, alias in SPEC_17_BENCHMARK.items():
                for prop in props:
                    value = method + "_SPEC17_" + alias + '_' + prop
                    if prop == "PERF":
                        data = data.replace(value, str(result_summary[idx][alias][0]))
                    elif prop == "POWER":
                        data = data.replace(value, str(result_summary[idx][alias][2]))
                    elif prop == "AREA":
                        data = data.replace(value, str(result_summary[idx][alias][3]))
                    else:
                        assert prop == "PPPA", \
                            assert_error("unknown prop: {}".format(prop))
                        data = data.replace(value, str(result_summary[idx][alias][4]))

        for method, idx in SPEC17_CONST.items():
            avg_perf, avg_power, avg_area, avg_pppa = [], [], [], []
            for benchmark, alias in SPEC_17_BENCHMARK.items():
                avg_perf.append(result_summary[idx][alias][0])
                avg_power.append(result_summary[idx][alias][2])
                avg_area.append(result_summary[idx][alias][3])
                avg_pppa.append(result_summary[idx][alias][4])
            avg_perf = np.average(avg_perf)
            avg_power = np.average(avg_power)
            avg_area = np.average(avg_area)
            avg_pppa = np.average(avg_pppa)
            for prop in props:
                value = method + "_SPEC17_" + prop
                if prop == "PERF":
                    data = data.replace(value, str(avg_perf))
                elif prop == "POWER":
                    data = data.replace(value, str(avg_power))
                elif prop == "AREA":
                    data = data.replace(value, str(avg_area))
                else:
                    assert prop == "PPPA", \
                        assert_error("unknown prop: {}".format(prop))
                    data = data.replace(value, str(avg_pppa))

    with open(os.path.join(cur_root, "{}.tex".format(name)), 'w') as f:
        f.write(data)

    cmd = "pdflatex {}".format(os.path.join(cur_root, "{}.tex".format(name)))
    execute(cmd)

    remove(os.path.join(cur_root, "{}.aux".format(name)))
    remove(os.path.join(cur_root, "{}.log".format(name)))


def main():
    simulate()
    result_summary = get_sim_result()
    plot(result_summary)


if __name__ == "__main__":
    archexplorer_root = os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        os.path.pardir,
        os.path.pardir
    )
    cur_root = os.path.join(
        os.path.dirname(os.path.abspath(__file__))
    )
    args = parse_multi_args()
    for arg in args.configs:
        if "spec06" in arg:
            exp_fig14_spec06_yml = arg
        elif "spec17" in arg:
            exp_fig14_spec17_yml = arg
    pat_ppa_rpt = re.compile(r"\d+\.\d+")
    main()
