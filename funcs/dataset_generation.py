# Author: baichen.bai@alibaba-inc.com


import os
import re
import sys
import numpy as np
from typing import List, Tuple
from utils.utils import if_exist, info, write_txt, execute
from funcs.design.o3cpu.o3cpu_design_space import parse_design_space


arch_explorer_root = os.path.abspath(
    os.path.join(
        os.path.dirname(__file__),
        os.path.pardir
    )
)

temp_root = os.path.abspath(
    os.path.join(
        arch_explorer_root,
        "temp"
    )
)

pat = re.compile(r"\d+\.\d+""")

def concat_arch_params(dataset: List, o3cpu_design_space: object, idx: int):
    embedding = o3cpu_design_space.idx_to_embedding(idx)
    for e in embedding:
        dataset.append(e)


def concat_benchmark_features(dataset: List, benchmark: str):
    report = os.path.abspath(
        os.path.join(
            arch_explorer_root, "infras", "deg", "report", benchmark
        )
    )
    # raw-length
    # raw-count
    # is-load
    # is-store
    # use-int-alu
    # use-int-mult-div
    # use-fp-alu
    # use-fp-mult-div
    # use-rd-wr-port
    # is-control
    # use-int-rf
    # use-fp-rf
    with open(report, 'r') as f:
        cnt = f.readlines()
        for i in range(len(cnt) - 1):
            dataset.append(int(cnt[i].split()[1]))


def concat_bottlenecks(dataset: List, bottleneck_rpt: str):
    with open(bottleneck_rpt, 'r') as f:
        cnt = f.readlines()
        idx = ["Bottleneck" in line for line in cnt].index(True)
        # Base
        # BP
        # ROB
        # LQ
        # SQ
        # INT-RF
        # FP-RF
        # IQ
        # IntAlu
        # IntMultDiv
        # FpAlu
        # FpMultDiv
        # RdWrPort
        # D-cache
        # RAW
        # I-cache
        # FetchBuffer
        # others
        for i in range(idx + 1, len(cnt) - 1):
            dataset.append(int(cnt[i].split()[1]))


def get_ppa(ppa_rpt: str):
    with open(ppa_rpt, 'r') as f:
        cnt = f.read()
    res = re.findall(pat, cnt)
    ipc = float(res[0])
    power = float(res[2])
    area = float(res[3])
    return ipc, power, area


def get_ppa_by_original_file(bmark_root: str):
    # performance
    stats = os.path.join(bmark_root, "stats.txt")
    pp = os.path.join(bmark_root, "report")
    ppa_rpt = os.path.join(bmark_root, "ppa.rpt")
    if not if_exist(ppa_rpt) and if_exist(stats) and not if_exist(pp):
        template = os.path.join(
            arch_explorer_root,
            "tools",
            "templates",
            "switch-o3cpu.xml"
        )
        cmd = "{} {} -c {} " \
            "-s {} " \
            "-t {} " \
            "-o {}".format(
                sys.executable,
                os.path.join(
                    arch_explorer_root,
                    "tools",
                    "gem5-mcpat-parser.py"
                ),
                os.path.join(bmark_root, "config.json"),
                os.path.join(bmark_root, "stats.txt"),
                template,
                os.path.join(bmark_root, "mcpat.xml")
            )
        cmd = "{} && {} -infile {} -print_level 5 > {}".format(
            cmd,
            os.path.join(
                os.path.join(
                    arch_explorer_root,
                    "infras",
                    "mcpat-research"
                ),
                "mcpat"
            ),
            os.path.join(bmark_root, "mcpat.xml"),
            os.path.join(bmark_root, "report")
        )
        execute(cmd)
    ipc, cpi = get_perf(stats)
    power, area = get_power_area(pp)
    with open(os.path.join(bmark_root, "ppa.rpt"), 'w') as f:
        msg = "IPC: {:.8f}, CPI: {:.8f}, Power: {:.8f}, area: {:.5f}".format(
            ipc, cpi, power, area
        )
        f.write(msg + '\n')
    return ipc, power, area


def get_perf(stats) -> Tuple[float, float]:
    ipc = cpi = 0
    if if_exist(stats):
        with open(stats, 'r') as f:
            cnt = f.readlines()
        cpu = "switch_cpus"
        for line in cnt:
            if line.startswith("system.{}.ipc".format(cpu)):
                ipc = float(line.split()[1])
                continue
            elif line.startswith("system.{}.cpi".format(cpu)):
                cpi = float(line.split()[1])
    return ipc, cpi


def get_power_area(report: str) -> Tuple[float, float] :
    p_subthreshold = re.compile(r"Subthreshold\ Leakage\ =\ [+-]?(\d+(\.\d*)?|\.\d+)([eE][+-]?\d+)?\ W")
    p_gate = re.compile(r"Gate\ Leakage\ =\ [+-]?(\d+(\.\d*)?|\.\d+)([eE][+-]?\d+)?\ W")
    p_dynamic = re.compile(r"Runtime\ Dynamic\ =\ [+-]?(\d+(\.\d*)?|\.\d+)([eE][+-]?\d+)?\ W")
    p_area = re.compile(r"Area\ =\ [+-]?(\d+(\.\d*)?|\.\d+)([eE][+-]?\d+)?\ mm\^2")
    subthreshold, gate, dynamic, area = 0, 0, 0, 0
    with open(report, 'r') as f:
        cnt = f.read()
        subthreshold = float(p_subthreshold.findall(cnt)[1][0])
        gate = float(p_gate.findall(cnt)[1][0])
        dynamic = float(p_dynamic.findall(cnt)[1][0])
        area = float(p_area.findall(cnt)[1][0])
    return subthreshold + gate + dynamic, area


def dataset_generation(configs: dict):
    o3cpu_design_space = parse_design_space(configs["design-space"])
    dataset = []

    simulators = os.listdir(temp_root)
    for simulator in simulators:
        if len(os.listdir(os.path.join(temp_root, simulator))) == 14:
            # 14 benchmarks of SPEC2017
            ipc, power, area, avg = 0, 0, 0, 0
            _dataset = []
            idx = simulator.split('-')[1]
            # architecture parameters
            concat_arch_params(_dataset, o3cpu_design_space, int(idx))
            for benchmark in os.listdir(os.path.join(temp_root, simulator)):
                ppa_rpt = os.path.join(temp_root, simulator, benchmark, "ppa.rpt")
                if if_exist(ppa_rpt):
                    _ipc, _power, _area = get_ppa(ppa_rpt)
                else:
                    _ipc, _power, _area = get_ppa_by_original_file(
                        os.path.join(temp_root, simulator, benchmark)
                    )
                ipc += _ipc
                power += _power
                area += _area
                avg += 1
            ipc /= avg
            power /= avg
            area /= avg
            _dataset.append(ipc)
            _dataset.append(power)
            _dataset.append(area)
            dataset.append(_dataset)
    dataset = np.array(dataset)
    write_txt(configs["dataset"]["output"], dataset, fmt="%f")
