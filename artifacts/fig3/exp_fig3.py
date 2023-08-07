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


CONST = 6


def conduct_dse():
    # check benchmark path
    configs = get_configs(artifacts_yml)
    if not if_exist(
        configs["spec2017"]["benchmark-root"], quiet=False
    ) or not if_exist(
        configs["spec2017"]["checkpoint-root"], quiet=False
    ):
        error("SPEC17 benchmark root directory path is not found. "
            "Please have a double-check.")

    # re-configurate gem5 & benchmark path
    with open(exp_fig3_yml, 'r') as fin:
        data = fin.read()
    with open(exp_fig3_yml, 'w') as fout:
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

    cmd = "{} {} -c {}".format(
        sys.executable,
        os.path.join(archexplorer_root, "main", "main.py"),
        os.path.join(cur_root, "configs", "exp-fig3.yml")
    )
    execute(cmd)


def get_sim_result():
    ppa_rpt = os.path.join(cur_root, "report-8")
    summary = os.path.join(cur_root, "summary")
    cmd = "mv {} {}".format(
            ppa_rpt, summary
        )
    execute(cmd)

    result_summary = []
    with open(summary, 'r') as f:
        content = f.readlines()
        for i in range(CONST):
            cnt = content[i]
            ipc = float(re.findall(r"ipc: (\d+\.\d+)", cnt)[0])
            power = float(re.findall(r"power: (\d+\.\d+)", cnt)[0])
            area = float(re.findall(r"area: (\d+\.\d+)", cnt)[0])
            pppa = float(re.findall(r"pppa: (\d+\.\d+)", cnt)[0])
            result_summary.append([ipc, power, area, pppa])

    return result_summary


def calc_ratio(val, base):
    return (val - base) / base + 1


def summarize_result(result_summary):
    # plot
    with open(os.path.join(cur_root, "fig3_latex_template.tex"), 'r') as f:
        data = f.read()

        for i in range(1, CONST + 1):
            data = data.replace("PPPA_" + str(i), str(result_summary[i - 1][-1]))
            data = data.replace("IPC_" + str(i), str(result_summary[i - 1][0]))
            data = data.replace("POWER_" + str(i), str(result_summary[i - 1][1]))
            data = data.replace("AREA_" + str(i), str(result_summary[i - 1][2]))

    with open(os.path.join(cur_root, "fig3_latex.tex"), 'w') as f:
        f.write(data)

    cmd = "pdflatex {}".format(os.path.join(cur_root, "fig3_latex.tex"))
    execute(cmd)

    remove(os.path.join(cur_root, "fig3_latex.aux"))
    remove(os.path.join(cur_root, "fig3_latex.log"))


def main():
    conduct_dse()
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
    artifacts_yml = os.path.join(
        archexplorer_root, "artifacts", "configs", "artifacts.yml"
    )
    args = parse_args()
    exp_fig3_yml = args.configs
    main()
