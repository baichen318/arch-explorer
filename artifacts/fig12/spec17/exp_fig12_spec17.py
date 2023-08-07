# Author: baichen.bai@alibaba-inc.com


import os
import re
import sys
import numpy as np
import matplotlib.pyplot as plt
from collections import OrderedDict
from utils.thread import WorkerThread
from utils.utils import get_configs_from_command, execute, \
    remove, mkdir, info, error, get_configs, if_exist
from funcs.sim.o3cpu.o3cpu_simulation import O3CPUSimulation
from funcs.design.o3cpu.o3cpu_design_space import parse_design_space


CONST = 8


class EOFException(Exception):
    def __init__(self, target):
        self.msg = "end of line: {}".format(target)

    def __str__(self):
        return self.msg


class Report(object):
    def __init__(self, name):
        super(Report, self).__init__()
        self.name = name
        f = open(name, 'r')
        self.content = f.readlines()
        self.fp = 0

    def incr_fp(self):
        self.fp += 1

    def eof(self):
        return self.fp == len(self.content)

    def read(self):
        return self.content[self.fp]

    def next(self):
        if not self.eof():
            content = self.read()
            while not self.valid(content):
                self.incr_fp()
                if not self.eof():
                    content = self.read()
                else:
                    break
            if not self.eof():
                self.incr_fp()
                return self.parse(content)
            else:
                raise EOFException(self.name)
        raise EOFException(self.name)


class ArchExplorerReport(Report):
    def __init__(self, name):
        super(ArchExplorerReport, self).__init__(name)

    def valid(self, content):
        if content == '\n' or \
            content.startswith("optimal solution"):
            return False
        return True

    def parse(self, content):
        microarch = eval(re.findall(r"\[[,\ \d]+\]", content)[0])
        ipc = float(re.findall(r"ipc: (\d+\.\d+)", content)[0])
        power = float(re.findall(r"power: (\d+\.\d+)", content)[0])
        area = float(re.findall(r"area: (\d+\.\d+)", content)[0])
        pppa = float(re.findall(r"pppa: (\d+\.\d+)", content)[0])
        return microarch, ipc, power, area, pppa


class ArchRankerReport(Report):
    def __init__(self, name):
        super(ArchRankerReport, self).__init__(name)

    def valid(self, content):
        if content == '\n' or \
            content.startswith("solutions:") or \
            (content.startswith("simulation count") and \
            "embedding" not in content):
            return False
        return True

    def parse(self, content):
        microarch = eval(re.findall(r"\[[,\ \d]+\]", content)[0])
        ipc = float(re.findall(r"ipc: (\d+\.\d+)", content)[0])
        power = float(re.findall(r"power: (\d+\.\d+)", content)[0])
        area = float(re.findall(r"area: (\d+\.\d+)", content)[0])
        return microarch, ipc, power, area


class AdaBoostReport(Report):
    def __init__(self, name):
        super(AdaBoostReport, self).__init__(name)

    def valid(self, content):
        if content == '\n' or \
            content.startswith("obtained solution:") or \
            content.startswith("simulation count"):
            return False
        return True

    def parse(self, content):
        ppa = content.split("ppa:")[-1]
        ppa = re.findall(r"(\d+\.\d+)", ppa)
        microarch = eval(re.findall(r"\[[,\ \d]+\]", content)[0])
        ipc = float(ppa[0])
        power = float(ppa[1])
        area = float(ppa[2])
        return microarch, ipc, power, area


class BOOMExplorerReport(object):
    def __init__(self, x_rpt, y_rpt):
        super(BOOMExplorerReport, self).__init__()
        self.x_rpt_name = x_rpt
        self.y_rpt_name = y_rpt
        f = open(self.x_rpt_name, 'r')
        self.x_content = f.readlines()
        f = open(self.y_rpt_name, 'r')
        self.y_content = f.readlines()
        self.fp = 0
        assert len(self.x_content) == \
            len(self.y_content), \
            error("invalid report from BOOM-Explorer.")

    def incr_fp(self):
        self.fp += 1

    def eof(self):
        return self.fp == len(self.x_content)

    def read(self):
        return self.x_content[self.fp], \
            self.y_content[self.fp]

    def valid(self, x):
        if x == '\n':
            return False
        return True

    def parse(self, x, y):
        microarch = [int(i) for i in x.split()]
        ipc, power, area = [float(i) for i in y.split()]
        return microarch, ipc, power, area

    def next(self):
        if not self.eof():
            x, y = self.read()
            while not self.valid(x):
                self.incr_fp()
                if not self.eof():
                    x, y = self.read()
                else:
                    break
            if not self.eof():
                self.incr_fp()
                return self.parse(x, y)
            else:
                raise EOFException(self.x_rpt_name)
        raise EOFException(self.x_rpt_name)


def conduct_archranker_dse():
    # check benchmark path
    configs = get_configs(artifacts_yml)
    if not if_exist(
        configs["simulation"]["benchmark"]["spec2017"]["benchmark-root"],
        quiet=False
    ) or not if_exist(
        configs["simulation"]["benchmark"]["spec2017"]["checkpoint-root"],
        quiet=False
    ):
        error("SPEC17 benchmark root directory path is not found. "
            "Please have a double-check.")

    # re-configurate gem5 & benchmark path
    exp_fig12_spec17_archranker_yml = os.path.join(
        cur_root, "configs", "exp-fig12-spec17-archranker-configs.yml"
    )
    with open(exp_fig12_spec06_archranker_yml, 'r') as fin:
        data = fin.read()
    with open(exp_fig12_spec06_archranker_yml, 'w') as fout:
        data = data.replace("/path/to/gem5-research-root",
            os.path.join(
                archexplorer_root,
                "infras",
                "gem5-research"
            )
        )
        data = data.replace("/path/to/spec2017",
            configs["simulation"]["benchmark"]["spec2017"]["benchmark-root"]
        )
        data = data.replace("/path/to/checkpoint",
            configs["simulation"]["benchmark"]["spec2017"]["checkpoint-root"]
        )
        fout.write(data)

    cmd = "cd {} && {} {} -s {} -c {}".format(
            os.path.join(archexplorer_root, "baseline", "archranker"),
            sys.executable,
            os.path.join(
                archexplorer_root,
                "baseline",
                "archranker",
                "archranker.py"
            ),
            os.path.join(
                cur_root,
                "configs",
                "exp-fig12-spec17-archranker-settings.yml"
            ),
            exp_fig12_spec17_archranker_yml
        )
    execute(cmd)


def conduct_adaboost_dse():
    # check benchmark path
    configs = get_configs(artifacts_yml)
    if not if_exist(
        configs["simulation"]["benchmark"]["spec2017"]["benchmark-root"],
        quiet=False
    ) or not if_exist(
        configs["simulation"]["benchmark"]["spec2017"]["checkpoint-root"],
        quiet=False
    ):
        error("SPEC17 benchmark root directory path is not found. "
            "Please have a double-check.")

    # re-configurate gem5 & benchmark path
    exp_fig12_spec17_adaboost_yml = os.path.join(
        cur_root, "configs", "exp-fig12-spec17-adaboost-configs.yml"
    )
    with open(exp_fig12_spec17_adaboost_yml, 'r') as fin:
        data = fin.read()
    with open(exp_fig12_spec17_adaboost_yml, 'w') as fout:
        data = data.replace("/path/to/gem5-research-root",
            os.path.join(
                archexplorer_root,
                "infras",
                "gem5-research"
            )
        )
        data = data.replace("/path/to/spec2017",
            configs["simulation"]["benchmark"]["spec2017"]["benchmark-root"]
        )
        data = data.replace("/path/to/checkpoint",
            configs["simulation"]["benchmark"]["spec2017"]["checkpoint-root"]
        )
        fout.write(data)


    cmd = "cd {} && {} {} -s {} -c {}".format(
            os.path.join(archexplorer_root, "baseline", "adaboost"),
            sys.executable,
            os.path.join(
                archexplorer_root,
                "baseline",
                "adaboost",
                "main.py"
            ),
            os.path.join(
                cur_root,
                "configs",
                "exp-fig12-spec17-adaboost-settings.yml"
            ),
            exp_fig12_spec17_adaboost_yml
        )
    execute(cmd)


def conduct_boom_explorer_dse():
    # check benchmark path
    configs = get_configs(artifacts_yml)
    if not if_exist(
        configs["simulation"]["benchmark"]["spec2017"]["benchmark-root"],
        quiet=False
    ) or not if_exist(
        configs["simulation"]["benchmark"]["spec2017"]["checkpoint-root"],
        quiet=False
    ):
        error("SPEC17 benchmark root directory path is not found. "
            "Please have a double-check.")

    # re-configurate gem5 & benchmark path
    exp_fig12_spec17_boom_explorer_yml = os.path.join(
        cur_root, "configs", "exp-fig12-spec17-boom-explorer-configs.yml"
    )
    with open(exp_fig12_spec17_boom_explorer_yml, 'r') as fin:
        data = fin.read()
    with open(exp_fig12_spec17_boom_explorer_yml, 'w') as fout:
        data = data.replace("/path/to/gem5-research-root",
            os.path.join(
                archexplorer_root,
                "infras",
                "gem5-research"
            )
        )
        data = data.replace("/path/to/spec2017",
            configs["simulation"]["benchmark"]["spec2017"]["benchmark-root"]
        )
        data = data.replace("/path/to/checkpoint",
            configs["simulation"]["benchmark"]["spec2017"]["checkpoint-root"]
        )
        fout.write(data)

    cmd = "cd {} && {} {} -s {} -c {}".format(
            os.path.join(archexplorer_root, "baseline", "boom_explorer"),
            sys.executable,
            os.path.join(
                archexplorer_root,
                "baseline",
                "boom_explorer",
                "main.py"
            ),
            os.path.join(
                cur_root,
                "configs",
                "exp-fig12-spec17-boom-explorer-settings.yml"
            ),
            exp_fig12_spec17_boom_explorer_yml
        )
    execute(cmd)


def conduct_arch_explorer_dse():
    # check benchmark path
    configs = get_configs(artifacts_yml)
    if not if_exist(
        configs["simulation"]["benchmark"]["spec2017"]["benchmark-root"],
        quiet=False
    ) or not if_exist(
        configs["simulation"]["benchmark"]["spec2017"]["checkpoint-root"],
        quiet=False
    ):
        error("SPEC17 benchmark root directory path is not found. "
            "Please have a double-check.")

    for i in range(1, CONST + 1):
        # re-configurate gem5 & benchmark path
        exp_fig12_spec17_arch_explorer_yml = os.path.join(
            cur_root, "configs",
            "exp-fig12-spec17-arch-explorer-p{}.yml".format(i)
        )
        with open(exp_fig12_spec17_arch_explorer_yml, 'r') as fin:
            data = fin.read()
        with open(exp_fig12_spec17_arch_explorer_yml, 'w') as fout:
            data = data.replace("/path/to/gem5-research-root",
                os.path.join(
                    archexplorer_root,
                    "infras",
                    "gem5-research"
                )
            )
            data = data.replace("/path/to/spec2017",
                configs["simulation"]["benchmark"]["spec2017"]["benchmark-root"]
            )
            data = data.replace("/path/to/checkpoint",
                configs["simulation"]["benchmark"]["spec2017"]["checkpoint-root"]
            )
            fout.write(data)

        cmd = "{} {} -c {}".format(
            sys.executable,
            os.path.join(archexplorer_root, "main", "main.py"),
            exp_fig12_spec17_arch_explorer_yml
        )
        execute(cmd)


def get_archranker_sim_result():
    report = ArchRankerReport(os.path.join(
            archexplorer_root, "baseline", "archranker",
            "reports", "report"
        )
    )

    result_summary = []
    while not report.eof():
        try:
            microarch, ipc, power, area = report.next()
            result_summary.append([microarch, ipc, power, area])
        except EOFException as e:
            break
    return result_summary


def get_adaboost_sim_result():
    report = AdaBoostReport(os.path.join(
            archexplorer_root, "baseline", "adaboost",
            "reports", "report"
        )
    )

    result_summary = []
    while not report.eof():
        try:
            microarch, ipc, power, area = report.next()
            result_summary.append([microarch, ipc, power, area])
        except EOFException as e:
            break
    return result_summary


def get_boom_explorer_sim_result():
    root = os.path.join(archexplorer_root, "baseline",
        "boom_explorer", "reports"
    )
    x_rpt = os.path.join(root, "x.rpt")
    y_rpt = os.path.join(root, "y.rpt")
    report = BOOMExplorerReport(x_rpt, y_rpt)

    result_summary = []
    while not report.eof():
        try:
            microarch, ipc, power, area = report.next()
            result_summary.append([microarch, ipc, power, area])
        except EOFException as e:
            break
    return result_summary


def if_traverse_all_reports(reports):
    ret = []
    # for i in range(CONST):
    for i in range(len([7])):
        ret.append(reports[i].eof())

    return all(ret)


def generate_yaml(microarch):
    configs = get_configs(os.path.join(
            cur_root, "configs", "exp-fig12-spec17-arch-explorer-p1.yml"
        )
    )
    return """
# Author: baichen.bai@alibaba-inc.com


## working mode specifications
# initialize | simulation | dataset-generation | exploration
# initialize: initialize the data set using RTED
mode: simulation


## design space specifications
design-space: design-space/design-space.xlsx


## simulation specifications
simulation:
  simulator:
    # gem5-research root path
    gem5-research-root: {}
    # choose the simulator to run
    start-idx: ~
    end-idx: ~
    candidate-idx: ~
    # select designs from design dataset
    candidate-design-set: ~
    candidate-embedding:
      - {}
  # benchmark specifications
  benchmark:
    # choose the benchmark to run
    run: spec2017
    spec2006:
      benchmark-root: ~
      candidate: ~
      # since we have no simpoints for SPEC2006, we use different max-insts
      # to test the benchmark. Fewer instructions are used in DSE.
      dse: False
    spec2017:
      benchmark-root: {}
      checkpoint-root: {}
      # maximum instruction
      max-insts: ~
      candidate: ~
    bare-model:
      warmup-insts: ~
      fast-forward: ~
      candidate: ~
  misc-setting:
    # True: use the new DEG to model, False: disable DEG modeling
    deg-model: False
    # True: enable visualization for DEG, False: disable visualization
    vis: False
    # specify the instruction sequence to view
    # the index is started with 1
    start-idx: ~
    end-idx: ~

""".format(
        configs["simulation"]["simulator"]["gem5-research-root"],
        microarch,
        configs["simulation"]["benchmark"]["spec2017"]["benchmark-root"],
        configs["simulation"]["benchmark"]["spec2017"]["checkpoint-root"],
    )


def re_simulate(microarch):
    # 1. generate template YAML
    yaml = generate_yaml(microarch)
    temp_root = os.path.join(cur_root, "temp")
    mkdir(temp_root)
    with open(os.path.join(temp_root, "temp.yml"), 'w') as f:
        f.write(yaml)
    # 2. execute the simulation
    cmd = "{} {} -c {}".format(
        sys.executable,
        os.path.join(archexplorer_root, "main", "main.py"),
        os.path.join(temp_root, "temp.yml")
    )
    execute(cmd)
    # 3. get the result
    configs = get_configs(os.path.join(temp_root, "temp.yml"))
    simulator = O3CPUSimulation(
        design_space=parse_design_space(
            configs["design-space"]
        ),
        configs=configs["simulation"]
    )

    idx = simulator.o3cpu_design_space.embedding_to_idx(
        configs["simulation"]["simulator"]["candidate-embedding"][0]
    )

    cmd = "{} {} -i {}".format(
        sys.executable,
        os.path.join(archexplorer_root, "tools", "get_ppa.py"),
        idx
    )
    execute(cmd)

    ppa_rpt = os.path.join(temp_root, "gem5-{}-ppa.rpt".format(idx))
    cmd = "find {} -name \"ppa.rpt\" -exec cat {{}} > {} \;".format(
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

    return ipc, cpi, power, area


def get_arch_explorer_sim_result():
    # collect results until we attain a number of simulations
    for i in range(1, CONST + 1):
        ppa_rpt = os.path.join(
            archexplorer_root, "main", "report-{}".format(i)
        )
        cmd = "mv -f {} {} && mv {} {}".format(
                ppa_rpt, os.path.join(cur_root),
                "report-{}".format(i),
                "spec17-report-{}".format(i)
            )
        execute(cmd)

    result_summary = []

    reports = []
    for i in range(1, CONST + 1):
        report = ArchExplorerReport("spec17-report-{}".format(i))
        reports.append(report)

    result_summary = []
    while not if_traverse_all_reports(reports):
        for i in range(CONST):
            try:
                microarch, ipc, power, area, pppa = reports[i].next()
                """
                    NOTICE: we need to re-simulate and get the PPA
                """
                ipc, cpi, power, area = re_simulate(microarch)
                pppa = (ipc * ipc) / (power * area)
                result_summary.append([microarch, ipc, power, area, pppa])
            except EOFException as e:
                continue

    return result_summary


def summarize_archranker_result(result_summary):
    # 1. generate PPA trace
    archranker_spec17_report = os.path.join(
        cur_root, "spec17-archranker-report"
    )
    archranker_spec17_pareto_root = os.path.join(
        cur_root, "spec17-archranker-pareto-hv-report"
    )
    with open(archranker_spec17_report, 'w') as f:
        f.write("IPC\tPower\tArea\n")
        for i in range(len(result_summary) - 1):
            result = result_summary[i]
            f.write("{}\t{}\t{}\n".format(result[1], result[2], result[3]))
        last_idx = len(result_summary) - 1
        result = result_summary[last_idx]
        f.write("{}\t{}\t{}".format(result[1], result[2], result[3]))

    # 2. generate Pareto hypervolume
    cmd = "{} {} -s {} -o {}".format(
            sys.executable,
            os.path.join(
                archexplorer_root,
                "tools",
                "pareto-hypervolume-curve.py"
            ),
            archranker_spec17_report,
            archranker_spec17_pareto_root
        )
    execute(cmd)

    # 3. revise generate Pareto hypervolume reports
    archranker_spec17_pareto_curve_report = os.path.join(
        archranker_spec17_pareto_root, "pareto-curve.rpt"
    )
    data_root = os.path.join(cur_root, "data")
    mkdir(data_root)
    archranker_spec17_pareto_curve_data = os.path.join(
        data_root, "archranker-pareto-curve.data"
    )
    with open(archranker_spec17_pareto_curve_report, 'r') as fin:
        with open(
            archranker_spec17_pareto_curve_data,
            'w'
        ) as fout:
            content = fin.readlines()
            fout.write("Design\tPV\n")
            idx = 1
            for i in range(len(content) - 1):
                fout.write("{}\t{}".format(idx, content[i]))
                idx += 1
            last_idx = len(content) - 1
            fout.write("{}\t{}".format(idx, content[last_idx]))
            info("generate archranker's Pareto curve: {}".format(
                    archranker_spec17_pareto_curve_data
                )
            )

    # 4. plot
    archranker_spec17_pareto_curve_pdf = os.path.join(
        cur_root, "archranker-spec17.pdf"
    )
    with open(archranker_spec17_pareto_curve_data, 'r') as f:
        content = f.readlines()[1:]
        x = []
        y = []
        for cnt in content:
            _cnt = cnt.strip().split('\t')
            x.append(int(_cnt[0]) * 14)
            y.append(float(_cnt[-1]))
        plt.plot(x, y, 's-', color = 'r',label="ArchRanker")
        plt.legend(loc = "best")
        plt.xlabel("Simulations")
        plt.ylabel("Pareto hypervolume")
        plt.grid(linestyle=':')
        plt.savefig(archranker_spec17_pareto_curve_pdf)
        info("generate archranker's Pareto curve pdf: {}".format(
                archranker_spec17_pareto_curve_pdf
            )
        )
        plt.show()


def summarize_adaboost_result(result_summary):
    # 1. generate PPA trace
    adaboost_spec17_report = os.path.join(
        cur_root, "spec17-adaboost-report"
    )
    adaboost_spec17_pareto_root = os.path.join(
        cur_root, "spec17-adaboost-pareto-hv-report"
    )
    with open(adaboost_spec17_report, 'w') as f:
        f.write("IPC\tPower\tArea\n")
        for i in range(len(result_summary) - 1):
            result = result_summary[i]
            f.write("{}\t{}\t{}\n".format(result[1], result[2], result[3]))
        last_idx = len(result_summary) - 1
        result = result_summary[last_idx]
        f.write("{}\t{}\t{}".format(result[1], result[2], result[3]))

    # 2. generate Pareto hypervolume
    cmd = "{} {} -s {} -o {}".format(
            sys.executable,
            os.path.join(
                archexplorer_root,
                "tools",
                "pareto-hypervolume-curve.py"
            ),
            adaboost_spec17_report,
            adaboost_spec17_pareto_root
        )
    execute(cmd)

    # 3. revise generate Pareto hypervolume reports
    adaboost_spec17_pareto_curve_report = os.path.join(
        adaboost_spec17_pareto_root, "pareto-curve.rpt"
    )
    data_root = os.path.join(cur_root, "data")
    mkdir(data_root)
    adaboost_spec17_pareto_curve_data = os.path.join(
        data_root, "adaboost-pareto-curve.data"
    )
    with open(adaboost_spec17_pareto_curve_report, 'r') as fin:
        with open(
            adaboost_spec17_pareto_curve_data,
            'w'
        ) as fout:
            content = fin.readlines()
            fout.write("Design\tPV\n")
            idx = 1
            for i in range(len(content) - 1):
                fout.write("{}\t{}".format(idx, content[i]))
                idx += 1
            last_idx = len(content) - 1
            fout.write("{}\t{}".format(idx, content[last_idx]))
            info("generate adaboost's Pareto curve: {}".format(
                    adaboost_spec17_pareto_curve_data
                )
            )

    # 4. plot
    adaboost_spec17_pareto_curve_pdf = os.path.join(
        cur_root, "adaboost-spec17.pdf"
    )
    with open(adaboost_spec17_pareto_curve_data, 'r') as f:
        content = f.readlines()[1:]
        x = []
        y = []
        for cnt in content:
            _cnt = cnt.strip().split('\t')
            x.append(int(_cnt[0]) * 14)
            y.append(float(_cnt[-1]))
        plt.plot(x, y, 's-', color = 'r',label="AdaBoost")
        plt.legend(loc = "best")
        plt.xlabel("Simulations")
        plt.ylabel("Pareto hypervolume")
        plt.grid(linestyle=':')
        plt.savefig(adaboost_spec17_pareto_curve_pdf)
        info("generate adaboost's Pareto curve pdf: {}".format(
                adaboost_spec17_pareto_curve_pdf
            )
        )
        plt.show()


def summarize_boom_explorer_result(result_summary):
    # 1. generate PPA trace
    boom_explorer_spec17_report = os.path.join(
        cur_root, "spec17-boom_explorer-report"
    )
    boom_explorer_spec17_pareto_root = os.path.join(
        cur_root, "spec17-boom_explorer-pareto-hv-report"
    )
    with open(boom_explorer_spec17_report, 'w') as f:
        f.write("IPC\tPower\tArea\n")
        for i in range(len(result_summary) - 1):
            result = result_summary[i]
            f.write("{}\t{}\t{}\n".format(result[1], result[2], result[3]))
        last_idx = len(result_summary) - 1
        result = result_summary[last_idx]
        f.write("{}\t{}\t{}".format(result[1], result[2], result[3]))

    # 2. generate Pareto hypervolume
    cmd = "{} {} -s {} -o {}".format(
            sys.executable,
            os.path.join(
                archexplorer_root,
                "tools",
                "pareto-hypervolume-curve.py"
            ),
            boom_explorer_spec17_report,
            boom_explorer_spec17_pareto_root
        )
    execute(cmd)

    # 3. revise generate Pareto hypervolume reports
    boom_explorer_spec17_pareto_curve_report = os.path.join(
        boom_explorer_spec17_pareto_root, "pareto-curve.rpt"
    )
    data_root = os.path.join(cur_root, "data")
    mkdir(data_root)
    boom_explorer_spec17_pareto_curve_data = os.path.join(
        data_root, "boom_explorer-pareto-curve.data"
    )
    with open(boom_explorer_spec17_pareto_curve_report, 'r') as fin:
        with open(
            boom_explorer_spec17_pareto_curve_data,
            'w'
        ) as fout:
            content = fin.readlines()
            fout.write("Design\tPV\n")
            idx = 1
            for i in range(len(content) - 1):
                fout.write("{}\t{}".format(idx, content[i]))
                idx += 1
            last_idx = len(content) - 1
            fout.write("{}\t{}".format(idx, content[last_idx]))
            info("generate boom_explorer's Pareto curve: {}".format(
                    boom_explorer_spec17_pareto_curve_data
                )
            )

    # 4. plot
    boom_explorer_spec17_pareto_curve_pdf = os.path.join(
        cur_root, "boom_explorer-spec17.pdf"
    )
    with open(boom_explorer_spec17_pareto_curve_data, 'r') as f:
        content = f.readlines()[1:]
        x = []
        y = []
        for cnt in content:
            _cnt = cnt.strip().split('\t')
            x.append(int(_cnt[0]) * 14)
            y.append(float(_cnt[-1]))
        plt.plot(x, y, 's-', color = 'r',label="BOOM-Explorer")
        plt.legend(loc = "best")
        plt.xlabel("Simulations")
        plt.ylabel("Pareto hypervolume")
        plt.grid(linestyle=':')
        plt.savefig(boom_explorer_spec17_pareto_curve_pdf)
        info("generate boom_explorer's Pareto curve pdf: {}".format(
                boom_explorer_spec17_pareto_curve_pdf
            )
        )
        plt.show()


def summarize_arch_explorer_result(result_summary):
    # 1. generate PPA trace
    archexplorer_spec17_report = os.path.join(
        cur_root, "spec17-archexplorer-report"
    )
    archexplorer_spec17_pareto_root = os.path.join(
        cur_root, "spec17-archexplorer-pareto-hv-report"
    )
    with open(archexplorer_spec17_report, 'w') as f:
        f.write("IPC\tPower\tArea\n")
        for i in range(len(result_summary) - 1):
            result = result_summary[i]
            f.write("{}\t{}\t{}\n".format(result[1], result[2], result[3]))
        last_idx = len(result_summary) - 1
        result = result_summary[last_idx]
        f.write("{}\t{}\t{}".format(result[1], result[2], result[3]))

    # 2. generate Pareto hypervolume
    cmd = "{} {} -s {} -o {}".format(
            sys.executable,
            os.path.join(
                archexplorer_root,
                "tools",
                "pareto-hypervolume-curve.py"
            ),
            archexplorer_spec17_report,
            archexplorer_spec17_pareto_root
        )
    execute(cmd)

    # 3. revise generate Pareto hypervolume reports
    archexplorer_spec17_pareto_curve_report = os.path.join(
        archexplorer_spec17_pareto_root, "pareto-curve.rpt"
    )
    data_root = os.path.join(cur_root, "data")
    mkdir(data_root)
    archexplorer_spec17_pareto_curve_data = os.path.join(
        data_root, "archexplorer-pareto-curve.data"
    )
    with open(archexplorer_spec17_pareto_curve_report, 'r') as fin:
        with open(
            archexplorer_spec17_pareto_curve_data,
            'w'
        ) as fout:
            content = fin.readlines()
            fout.write("Design\tPV\n")
            idx = 1
            for i in range(len(content) - 1):
                fout.write("{}\t{}".format(idx, content[i]))
                idx += 1
            last_idx = len(content) - 1
            fout.write("{}\t{}".format(idx, content[last_idx]))
            info("generate archexplorer's Pareto curve: {}".format(
                    archexplorer_spec17_pareto_curve_data
                )
            )

    # 4. plot
    archexplorer_spec17_pareto_curve_pdf = os.path.join(
        cur_root, "archexplorer-spec17.pdf"
    )
    with open(archexplorer_spec17_pareto_curve_data, 'r') as f:
        content = f.readlines()[1:]
        x = []
        y = []
        for cnt in content:
            _cnt = cnt.strip().split('\t')
            x.append(int(_cnt[0]) * 14)
            y.append(float(_cnt[-1]))
        plt.plot(x, y, 's-', color = 'r',label="ArchExplorer")
        plt.legend(loc = "best")
        plt.xlabel("Simulations")
        plt.ylabel("Pareto hypervolume")
        plt.grid(linestyle=':')
        plt.savefig(archexplorer_spec17_pareto_curve_pdf)
        info("generate archexplorer's Pareto curve pdf: {}".format(
                archexplorer_spec17_pareto_curve_pdf
            )
        )
        plt.show()


def reproduce_archranker():
    conduct_archranker_dse()
    result_summary = get_archranker_sim_result()
    summarize_archranker_result(result_summary)


def reproduce_adaboost():
    conduct_adaboost_dse()
    result_summary = get_adaboost_sim_result()
    summarize_adaboost_result(result_summary)


def reproduce_boom_explorer():
    conduct_boom_explorer_dse()
    result_summary = get_boom_explorer_sim_result()
    summarize_boom_explorer_result(result_summary)


def reproduce_arch_explorer():
    conduct_arch_explorer_dse()
    result_summary = get_arch_explorer_sim_result()
    summarize_arch_explorer_result(result_summary)


def main():
    reproduce_archranker()
    reproduce_adaboost()
    reproduce_boom_explorer()
    reproduce_arch_explorer()


if __name__ == "__main__":
    archexplorer_root = os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        os.path.pardir,
        os.path.pardir,
        os.path.pardir
    )
    cur_root = os.path.join(
        os.path.dirname(os.path.abspath(__file__))
    )
    pat_ppa_rpt = re.compile(r"\d+\.\d+")
    artifacts_yml = os.path.join(
        archexplorer_root, "artifacts", "configs", "artifacts.yml"
    )
    main()
