# Author: baichen.bai@alibaba-inc.com


import os
import re
import sys
import numpy as np
from collections import OrderedDict
from funcs.sim.o3cpu.o3cpu_simulation import O3CPUSimulation
from funcs.design.o3cpu.o3cpu_design_space import parse_design_space
from utils.utils import execute, remove, if_exist, get_configs, error, \
    parse_args


MACROS = OrderedDict({
    7: "BASE",
    23: "ROB",
    39: "INT_RF",
    71: "FP_RF",
    135: "IQ",
    263: "LQ",
    519: "SQ",
    1031: "INT_ALU",
    2055: "INT_MULT_DIV",
    4103: "FP_ALU",
    8199: "FP_MULT_DIV",
    16391: "I_SZ",
    32775: "I_ASSOC",
    65543: "D_SZ",
    131079: "D_ASSOC"
})


def simulate():
    # check benchmark path
    configs = get_configs(exp_fig2_yml)
    spec06 = False if "demo" in exp_fig2_yml else True

    if spec06:
        if not if_exist(
            configs["simulation"]["benchmark"]["spec2006"]["benchmark-root"],
            quiet=False
        ):
            error("SPEC06 benchmark root directory path is not found. "
                "Please have a double-check (could docker mapping be incorrect?).")

    if not if_exist(
        configs["simulation"]["simulator"]["gem5-research-root"],
        quiet=False
    ):
        error("\"gem5-research\" root directory path is not found. "
            "Please have a double-check (could docker mapping be incorrect?).")

    cmd = "{} {} -c {}".format(
        sys.executable,
        os.path.join(archexplorer_root, "main", "main.py"),
        exp_fig2_yml
    )
    execute(cmd)


def get_sim_result():
    configs = get_configs(exp_fig2_yml)
    simulator = O3CPUSimulation(
        design_space=parse_design_space(
            configs["design-space"]
        ),
        configs=configs["simulation"]
    )

    arch_idx = []
    for embedding in configs["simulation"]["simulator"]["candidate-embedding"]:
        arch_idx.append(
            simulator.o3cpu_design_space.embedding_to_idx(embedding)
        )

    remove(ppa_rpt)

    result_summary = OrderedDict()
    for idx in arch_idx:
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

        cmd = "find {} -name \"ppa.rpt\" -exec cat {{}} >> {} \;".format(
                os.path.join(
                    archexplorer_root, "temp", "gem5-{}".format(idx)
                ),
                ppa_rpt
            )
        execute(cmd)

        """
            Refer it to: `get_simulation_results`
            /path/to/arch-explorer/algo/dse.py
        """
        with open(ppa_rpt, 'r') as f:
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

            result_summary[idx] = [ipc, cpi, power, area]

    return result_summary


def calc_ratio(val, base):
    return (val - base) / base + 1


def summarize_result(result_summary):
    # plot
    with open(os.path.join(cur_root, "fig2_template.tex"), 'r') as f:
        data = f.read()

        base = -1
        for k, v in MACROS.items():
            if v =="BASE":
                base = k

        base_perf = result_summary[k][0]
        base_power = result_summary[k][2]
        base_area = result_summary[k][3]
        for k, v in MACROS.items():
            if k != base:
                perf = result_summary[k][0]
                power = result_summary[k][2]
                area = result_summary[k][3]
                data = data.replace(v + "_PERF", str(calc_ratio(
                            result_summary[k][0], base_perf
                        )
                    )
                )
                data = data.replace(v + "_POWER", str(calc_ratio(
                        result_summary[k][2], base_power
                        )
                    )
                )
                data = data.replace(v + "_AREA", str(calc_ratio(
                        result_summary[k][3], base_area
                        )
                    )
                )
                data = data.replace(v + "_PPPA", str(calc_ratio(
                        (perf * perf) / (power * area),
                        (base_perf * base_perf) / (base_power * base_area)
                        )
                    )
                )

    if "demo" in exp_fig2_yml:
        name = "fig2-demo"
    elif "partial" in exp_fig2_yml:
        name = "fig2-partial"
    else:
        name = "fig2-full"
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
    ppa_rpt = os.path.join(cur_root, "ppa.rpt")
    pat_ppa_rpt = re.compile(r"\d+\.\d+")
    args = parse_args()
    exp_fig2_yml = args.configs
    main()
