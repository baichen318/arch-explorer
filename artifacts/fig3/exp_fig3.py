# Author: baichen.bai@alibaba-inc.com


import os
import re
import sys
import numpy as np
from collections import OrderedDict
from funcs.sim.o3cpu.o3cpu_simulation import O3CPUSimulation
from funcs.design.o3cpu.o3cpu_design_space import parse_design_space
from utils.utils import get_configs_from_command, get_configs, execute, \
    remove, if_exist, error, parse_args


CONST = [
    583789636473768,
    727075573990776,
    876587108988744,
    577855625635584,
    577952821368504,
    578050021271304
]


def simulate():
    # check benchmark path
    configs = get_configs(exp_fig3_yml)
    if not if_exist(
        configs["simulation"]["benchmark"]["spec2017"]["benchmark-root"],
        quiet=False
    ) or not if_exist(
        configs["simulation"]["benchmark"]["spec2017"]["checkpoint-root"],
        quiet=False
    ):
        error("SPEC17 benchmark root directory path is not found. "
            "Please have a double-check (could docker mapping be incorrect?).")

    cmd = "{} {} -c {}".format(
        sys.executable,
        os.path.join(archexplorer_root, "main", "main.py"),
        os.path.join(exp_fig3_yml)
    )
    execute(cmd)


def get_sim_result():
    summary = os.path.join(cur_root, "summary")

    result_summary = []
    for idx in CONST:
        path = os.path.join(
            archexplorer_root, "temp", "gem5-{}".format(idx)
        )
        if len(os.listdir(path)) == 0:
            error("GEM5 simulator gem5-{} compilation/simulation failed. Please check "
                "your environment to make sure GEM5 can be successfully "
                "compiled and simulated.".format(idx))
        cmd = "{} {} -i {}".format(
            sys.executable,
            os.path.join(archexplorer_root, "tools", "get_ppa.py"),
            idx
        )
        execute(cmd)

        remove(summary)
        cmd = "find {} -name \"ppa.rpt\" -exec cat {{}} >> {} \;".format(
                os.path.join(
                    archexplorer_root, "temp", "gem5-{}".format(idx)
                ),
                summary
            )
        execute(cmd)

        """
            Refer it to: `get_simulation_results`
            /path/to/arch-explorer/algo/dse.py
        """
        with open(summary, 'r') as f:
            ipc, cpi, power, area = [], [], [], []
            content = f.readlines()
            for cnt in content:
                result = re.findall(pat_ppa_rpt, cnt)
                _ipc = float(result[0])
                _cpi = float(result[1])
                _power = float(result[2])
                _area = float(result[3])
                ipc.append(_ipc)
                cpi.append(_cpi)
                power.append(_power)
                area.append(_area)

            ipc = np.average(ipc)
            cpi = np.average(cpi)
            power = np.average(power)
            area = np.average(area)
            pppa = (ipc * ipc) / (power * area)

            result_summary.append([ipc, cpi, power, area, pppa])
    remove(summary)

    return result_summary


def calc_ratio(val, base):
    return (val - base) / base + 1


def summarize_result(result_summary):
    # plot
    with open(os.path.join(cur_root, "fig3_template.tex"), 'r') as f:
        data = f.read()

        for i in range(1, len(CONST) + 1):
            data = data.replace("PPPA_" + str(i), str(result_summary[i - 1][-1]))
            data = data.replace("IPC_" + str(i), str(result_summary[i - 1][0]))
            data = data.replace("POWER_" + str(i), str(result_summary[i - 1][2]))
            data = data.replace("AREA_" + str(i), str(result_summary[i - 1][3]))

    name = "fig3"
    with open(os.path.join(cur_root, "{}.tex".format(name)), 'w') as f:
        f.write(data)

    cmd = "pdflatex {}".format(os.path.join(cur_root, "{}.tex".format(name)))
    execute(cmd)

    remove(os.path.join(cur_root, "{}.aux".format(name)))
    remove(os.path.join(cur_root, "{}.log".format(name)))


def main():
    simulate()
    result_summary = get_sim_result()
    summarize_result(result_summary)


if __name__ == "__main__":
    archexplorer_root = os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        os.path.pardir,
        os.path.pardir
    )
    cur_root = os.path.join(
        os.path.dirname(os.path.abspath(__file__))
    )
    pat_ppa_rpt = re.compile(r"\d+\.\d+")
    args = parse_args()
    exp_fig3_yml = args.configs
    main()
