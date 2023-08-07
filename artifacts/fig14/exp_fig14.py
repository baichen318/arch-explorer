# Author: baichen.bai@alibaba-inc.com


import os
import re
import sys
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


SPEC06 = OrderedDict({
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


SPEC17 = OrderedDict({
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


def simulate():
    # SPEC06

    # check benchmark path
    configs = get_configs(artifacts_yml)
    if not if_exist(
        configs["spec2006"]["benchmark-root"],
        quiet=False
    ):
        error("SPEC06 benchmark root directory path is not found. "
            "Please have a double-check.")
    if not if_exist(
        configs["spec2017"]["benchmark-root"],
        quiet=False
    ) or not if_exist(
        configs["spec2017"]["checkpoint-root"],
        quiet=False
    ):
        error("SPEC17 benchmark root directory path is not found. "
            "Please have a double-check.")

    # re-configurate gem5 & benchmark path
    with open(exp_spec06_fig14_yml, 'r') as fin:
        data = fin.read()
    with open(exp_spec06_fig14_yml, 'w') as fout:
        data = data.replace("/path/to/gem5-research-root",
            os.path.join(
                archexplorer_root,
                "infras",
                "gem5-research"
            )
        )
        data = data.replace("/path/to/spec2006",
            configs["spec2006"]["benchmark-root"]
        )
        fout.write(data)

    cmd = "{} {} -c {}".format(
        sys.executable,
        os.path.join(archexplorer_root, "main", "main.py"),
        exp_spec06_fig14_yml
    )
    execute(cmd)

    # SPEC17

    # re-configurate gem5 & benchmark path
    with open(exp_spec17_fig14_yml, 'r') as fin:
        data = fin.read()
    with open(exp_spec17_fig14_yml, 'w') as fout:
        data = data.replace("/path/to/gem5-research-root",
            os.path.join(
                archexplorer_root,
                "infras",
                "gem5-research"
            )
        )
        data = data.replace("/path/to/spec2017",
            configs["spec2017"]["benchmark-root"]
        )
        data = data.replace("/path/to/checkpoint",
            configs["spec2017"]["checkpoint-root"]
        )
        fout.write(data)

    # check benchmark path
    cmd = "{} {} -c {}".format(
        sys.executable,
        os.path.join(archexplorer_root, "main", "main.py"),
        exp_spec17_fig14_yml
    )
    execute(cmd)


def get_sim_result():
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

        for benchmark, alias in SPEC06.items():
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

        for benchmark, alias in SPEC17.items():
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
    props = [
        "PERF", "POWER", "AREA", "PPPA"
    ]
    with open(os.path.join(cur_root, "fig14_latex_template.tex"), 'r') as f:
        data = f.read()

        for method, idx in SPEC06_CONST.items():
            for benchmark, alias in SPEC06.items():
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
            for benchmark, alias in SPEC06.items():
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
            for benchmark, alias in SPEC17.items():
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
            for benchmark, alias in SPEC17.items():
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

    with open(os.path.join(cur_root, "fig14_latex.tex"), 'w') as f:
        f.write(data)

    cmd = "pdflatex {}".format(os.path.join(cur_root, "fig14_latex.tex"))
    execute(cmd)

    remove(os.path.join(cur_root, "fig14_latex.aux"))
    remove(os.path.join(cur_root, "fig14_latex.log"))


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
    artifacts_yml = os.path.join(
        archexplorer_root, "artifacts", "configs", "artifacts.yml"
    )
    exp_spec06_fig14_yml = os.path.join(
        cur_root, "configs", "exp-spec06-fig14.yml"
    )
    exp_spec17_fig14_yml = os.path.join(
        cur_root, "configs", "exp-spec17-fig14.yml"
    )
    pat_ppa_rpt = re.compile(r"\d+\.\d+")
    main()
