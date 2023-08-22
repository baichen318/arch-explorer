# Author: baichen.bai@alibaba-inc.com


import os
import re
import sys
import numpy as np
import matplotlib.pyplot as plt
from collections import OrderedDict
from utils.utils import get_configs_from_command, execute, \
    remove, mkdir, info, error, if_exist, copy
from funcs.sim.o3cpu.o3cpu_simulation import O3CPUSimulation
from funcs.design.o3cpu.o3cpu_design_space import parse_design_space


def get_ppa(content):
    perf,power, area = [float(i) for i in content.strip().split()]
    return perf, power, area


def summarize_archranker_result():
    mkdir(os.path.join(cur_root, "data"))
    raw_pf_rpt = os.path.join(
        fig12_root, "spec06-archranker-pareto-hv-report",
        "pareto-frontier.rpt"
    )
    orig_pf_rpt = os.path.join(cur_root, "data",
        "archranker-orig-pareto-frontier.data"
    )
    pf_rpt = os.path.join(cur_root, "data",
        "archranker-pareto-frontier.data"
    )
    f_orig_pf_rpt = open(orig_pf_rpt, 'w')
    f_pf_rpt = open(pf_rpt, 'w')
    with open(raw_pf_rpt, 'r') as fin:
        content = fin.readlines()
        f_orig_pf_rpt.write("IPC\tPower\tArea\n")
        f_pf_rpt.write("IPC\tPower\tArea\n")
        for cnt in content:
            f_orig_pf_rpt.write("{}".format(cnt))
            perf, power, area = get_ppa(cnt)
            f_pf_rpt.write("{}\t{}\t{}\n".format(perf, 1 / power, 1 / area))
        info("generate archranker's original Pareto frontier: {}".format(
                orig_pf_rpt
            )
        )
        info("generate archranker's Pareto frontier: {}".format(
                pf_rpt
            )
        )

    # copy all data points
    raw_data = os.path.join(fig12_root, "spec06-archranker-report")
    orig_data = os.path.join(cur_root, "data", "archranker-orig-ppa.data")
    data = os.path.join(cur_root, "data", "archranker-ppa.data")
    copy(raw_data, orig_data)
    with open(orig_data, 'r') as fin:
        with open(data, 'w') as fout:
            content = fin.readlines()
            fout.write("IPC\tPower\tArea\n")
            for cnt in content[1:]:
                perf, power, area = get_ppa(cnt)
                fout.write("{}\t{}\t{}\n".format(perf, 1 / power, 1 / area))
            info("generate archranker's original PPA: {}".format(
                    orig_data
                )
            )
            info("generate archranker's PPA: {}".format(
                    data
                )
            )


def summarize_adaboost_result():
    mkdir(os.path.join(cur_root, "data"))
    raw_pf_rpt = os.path.join(
        fig12_root, "spec06-adaboost-pareto-hv-report",
        "pareto-frontier.rpt"
    )
    orig_pf_rpt = os.path.join(cur_root, "data",
        "adaboost-orig-pareto-frontier.data"
    )
    pf_rpt = os.path.join(cur_root, "data",
        "adaboost-pareto-frontier.data"
    )
    f_orig_pf_rpt = open(orig_pf_rpt, 'w')
    f_pf_rpt = open(pf_rpt, 'w')
    with open(raw_pf_rpt, 'r') as fin:
        content = fin.readlines()
        f_orig_pf_rpt.write("IPC\tPower\tArea\n")
        f_pf_rpt.write("IPC\tPower\tArea\n")
        for cnt in content:
            f_orig_pf_rpt.write("{}".format(cnt))
            perf, power, area = get_ppa(cnt)
            f_pf_rpt.write("{}\t{}\t{}\n".format(perf, 1 / power, 1 / area))
        info("generate adaboost's original Pareto frontier: {}".format(
                orig_pf_rpt
            )
        )
        info("generate adaboost's Pareto frontier: {}".format(
                pf_rpt
            )
        )

    # copy all data points
    raw_data = os.path.join(fig12_root, "spec06-adaboost-report")
    orig_data = os.path.join(cur_root, "data", "adaboost-orig-ppa.data")
    data = os.path.join(cur_root, "data", "adaboost-ppa.data")
    copy(raw_data, orig_data)
    with open(orig_data, 'r') as fin:
        with open(data, 'w') as fout:
            content = fin.readlines()
            fout.write("IPC\tPower\tArea\n")
            for cnt in content[1:]:
                perf, power, area = get_ppa(cnt)
                fout.write("{}\t{}\t{}\n".format(perf, 1 / power, 1 / area))
            info("generate adaboost's original PPA: {}".format(
                    orig_data
                )
            )
            info("generate adaboost's PPA: {}".format(
                    data
                )
            )


def summarize_boom_explorer_result():
    mkdir(os.path.join(cur_root, "data"))
    raw_pf_rpt = os.path.join(
        fig12_root, "spec06-boom_explorer-pareto-hv-report",
        "pareto-frontier.rpt"
    )
    orig_pf_rpt = os.path.join(cur_root, "data",
        "boom-explorer-orig-pareto-frontier.data"
    )
    pf_rpt = os.path.join(cur_root, "data",
        "boom-explorer-pareto-frontier.data"
    )
    f_orig_pf_rpt = open(orig_pf_rpt, 'w')
    f_pf_rpt = open(pf_rpt, 'w')
    with open(raw_pf_rpt, 'r') as fin:
        content = fin.readlines()
        f_orig_pf_rpt.write("IPC\tPower\tArea\n")
        f_pf_rpt.write("IPC\tPower\tArea\n")
        for cnt in content:
            f_orig_pf_rpt.write("{}".format(cnt))
            perf, power, area = get_ppa(cnt)
            f_pf_rpt.write("{}\t{}\t{}\n".format(perf, 1 / power, 1 / area))
        info("generate boom-explorer's original Pareto frontier: {}".format(
                orig_pf_rpt
            )
        )
        info("generate boom-explorer's Pareto frontier: {}".format(
                pf_rpt
            )
        )

    # copy all data points
    raw_data = os.path.join(fig12_root, "spec06-boom_explorer-report")
    orig_data = os.path.join(cur_root, "data", "boom-explorer-orig-ppa.data")
    data = os.path.join(cur_root, "data", "boom-explorer-ppa.data")
    copy(raw_data, orig_data)
    with open(orig_data, 'r') as fin:
        with open(data, 'w') as fout:
            content = fin.readlines()
            fout.write("IPC\tPower\tArea\n")
            for cnt in content[1:]:
                perf, power, area = get_ppa(cnt)
                fout.write("{}\t{}\t{}\n".format(perf, 1 / power, 1 / area))
            info("generate boom-explorer's original PPA: {}".format(
                    orig_data
                )
            )
            info("generate boom-explorer's PPA: {}".format(
                    data
                )
            )


def summarize_arch_explorer_result():
    mkdir(os.path.join(cur_root, "data"))
    raw_pf_rpt = os.path.join(
        fig12_root, "spec06-archexplorer-pareto-hv-report",
        "pareto-frontier.rpt"
    )
    orig_pf_rpt = os.path.join(cur_root, "data",
        "arch-explorer-orig-pareto-frontier.data"
    )
    pf_rpt = os.path.join(cur_root, "data",
        "arch-explorer-pareto-frontier.data"
    )
    f_orig_pf_rpt = open(orig_pf_rpt, 'w')
    f_pf_rpt = open(pf_rpt, 'w')
    with open(raw_pf_rpt, 'r') as fin:
        content = fin.readlines()
        f_orig_pf_rpt.write("IPC\tPower\tArea\n")
        f_pf_rpt.write("IPC\tPower\tArea\n")
        for cnt in content:
            f_orig_pf_rpt.write("{}".format(cnt))
            perf, power, area = get_ppa(cnt)
            f_pf_rpt.write("{}\t{}\t{}\n".format(perf, 1 / power, 1 / area))
        info("generate arch-explorer's original Pareto frontier: {}".format(
                orig_pf_rpt
            )
        )
        info("generate arch-explorer's Pareto frontier: {}".format(
                pf_rpt
            )
        )

    # copy all data points
    raw_data = os.path.join(fig12_root, "spec06-archexplorer-report")
    orig_data = os.path.join(cur_root, "data", "arch-explorer-orig-ppa.data")
    data = os.path.join(cur_root, "data", "arch-explorer-ppa.data")
    copy(raw_data, orig_data)
    with open(orig_data, 'r') as fin:
        with open(data, 'w') as fout:
            content = fin.readlines()
            fout.write("IPC\tPower\tArea\n")
            for cnt in content[1:]:
                perf, power, area = get_ppa(cnt)
                fout.write("{}\t{}\t{}\n".format(perf, 1 / power, 1 / area))
            info("generate arch-explorer's original PPA: {}".format(
                    orig_data
                )
            )
            info("generate arch-explorer's PPA: {}".format(
                    data
                )
            )


def reproduce_archranker():
    summarize_archranker_result()


def reproduce_adaboost():
    summarize_adaboost_result()


def reproduce_boom_explorer():
    summarize_boom_explorer_result()


def reproduce_arch_explorer():
    summarize_arch_explorer_result()


def check_dse_result():
    """
        Fig. 13 is based on Fig. 12. So, Fig. 12
        should be produced from Fig. 13.
        Before we produce Fig. 13, we should make sure
        that following files are already generated.
        1. /path/to/fig12/spec06-adaboost-pareto-hv-report/pareto-frontier.rpt
        2. /path/to/fig12/spec06-archranker-archexplorer-pareto-hv-report/pareto-frontier.rpt
        3. /path/to/fig12/spec06-boom_explorer-pareto-hv-report/pareto-frontier.rpt
        4. /path/to/fig12/spec06-archexplorer-pareto-hv-report/pareto-frontier.rpt
        5. /path/to/fig12/spec06-adaboost-report
        6. /path/to/fig12/spec06-archranker-report
        7. /path/to/fig12/spec06-boom_explorer-report
        8. /path/to/fig12/spec06-archexplorer-report
    """
    method_name = ["adaboost", "archranker",
        "boom_explorer", "archexplorer"
        ]
    f = "pareto-frontier.rpt"

    # 1st check
    for method in method_name:
        report = os.path.join(
                fig12_root, "spec06-{}-pareto-hv-report".format(
                    method
                ), f
            )
        if_exist(report, strict=True, quiet=False)

    # 2nd check
    for method in method_name:
        report = os.path.join(
                fig12_root, "spec06-{}-report".format(
                        method
                    )
            )
        if_exist(report, strict=True, quiet=False)


def scatter_plot():
    # plot
    methods = OrderedDict({
        "ARCHRANKER": "archranker",
        "ADABOOST": "adaboost",
        "BOOM_EXPLORER": "boom-explorer",
        "ARCH_EXPLORER": "arch-explorer"
    })
    props = OrderedDict({
        "PPA": "ppa",
        "PARETO": "pareto-frontier",
        "ORIG_PPA": "orig-ppa",
        "ORIG_PARETO": "orig-pareto-frontier"
    })
    with open(os.path.join(cur_root, "fig13_template.tex"), 'r') as f:
        data = f.read()

        for method, m_alias in methods.items():
            for prop, p_alias in props.items():
                data = data.replace(method + '_' + prop,
                        m_alias + '-' + p_alias + ".data"
                    )

    name = "fig13"
    with open(os.path.join(cur_root, "{}.tex".format(name)), 'w') as f:
        f.write(data)

    cmd = "pdflatex {}".format(os.path.join(cur_root, "{}.tex".format(name)))
    execute(cmd)

    remove(os.path.join(cur_root, "{}.aux".format(name)))
    remove(os.path.join(cur_root, "{}.log".format(name)))


def main():
    check_dse_result()
    reproduce_archranker()
    reproduce_adaboost()
    reproduce_boom_explorer()
    reproduce_arch_explorer()
    scatter_plot()


if __name__ == "__main__":
    archexplorer_root = os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        os.path.pardir,
        os.path.pardir
    )
    cur_root = os.path.join(
        os.path.dirname(os.path.abspath(__file__))
    )
    # Fig. 13 only plots results for SPEC CPU2006
    fig12_root = os.path.join(
        archexplorer_root, "artifacts",
        "fig12", "spec06"
    )
    main()
