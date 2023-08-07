# Author: baichen.bai@alibaba-inc.com


import os
import re
import sys
import argparse
from typing import Tuple, NoReturn
from utils.utils import if_exist, info, warn


def parse_args():
    def initialize_parser(parser):
        parser.add_argument(
            "-i", "--idx",
            required=True,
            type=str,
            help="architecture index specification"
        )
        parser.add_argument(
            "-f", "--fast-forward",
            action="store_true",
            help="fast-forward specification"
        )
        return parser

    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser = initialize_parser(parser)
    return parser.parse_args()


def get_perf(stats: str, ff=False) -> Tuple[float, float]:
    """
        Refer it to:
        /path/to/arch-explorer/funcs/sim/o3cpu/o3cpu_simulation.py
    """

    ipc = cpi = 0
    if if_exist(stats):
        with open(stats, 'r') as f:
            cnt = f.readlines()
        """
            Use different CPUs based on the fast forwarding.
        """
        if ff:
            # fast-forward
            cpu = "cpu"
        else:
            cpu = "switch_cpus"
        for line in cnt:
            if line.startswith("system.{}.ipc".format(cpu)):
                ipc = float(line.split()[1])
                continue
            elif line.startswith("system.{}.cpi".format(cpu)):
                cpi = float(line.split()[1])
    return ipc, cpi


def get_power_area(report: str) -> Tuple[float, float]:
    """
        Refer it to:
        /path/to/arch-explorer/funcs/sim/o3cpu/o3cpu_simulation.py
    """

    p_subthreshold = re.compile(r"Subthreshold\ Leakage\ =\ [+-]?(\d+(\.\d*)?|\.\d+)([eE][+-]?\d+)?\ W")
    p_gate = re.compile(r"Gate\ Leakage\ =\ [+-]?(\d+(\.\d*)?|\.\d+)([eE][+-]?\d+)?\ W")
    p_dynamic = re.compile(r"Runtime\ Dynamic\ =\ [+-]?(\d+(\.\d*)?|\.\d+)([eE][+-]?\d+)?\ W")
    p_area = re.compile(r"Area\ =\ [+-]?(\d+(\.\d*)?|\.\d+)([eE][+-]?\d+)?\ mm\^2")
    subthreshold, gate, dynamic, area = 0, 0, 0, 0
    if if_exist(report):
        with open(report, 'r') as f:
            cnt = f.read()
            try:
                subthreshold = float(p_subthreshold.findall(cnt)[1][0])
                gate = float(p_gate.findall(cnt)[1][0])
                dynamic = float(p_dynamic.findall(cnt)[1][0])
                area = float(p_area.findall(cnt)[1][0])
            except Exception as e:
                info("McPAT report {} is failed to generate.".format(report))
        return subthreshold + gate + dynamic, area


def generate_report(path, ff) -> NoReturn:
    """
        Refer it to:
        /path/to/arch-explorer/funcs/sim/o3cpu/o3cpu_simulation.py
    """

    m5out = os.path.join(path)
    stats = os.path.join(m5out, "stats.txt")
    report = os.path.join(m5out, "report")
    ppa_rpt = os.path.join(m5out, "ppa.rpt")
    ipc, cpi = get_perf(stats, ff)
    power, area = get_power_area(report)
    with open(ppa_rpt, 'w') as f:
        msg = "IPC: {:.8f}, CPI: {:.8f}, " \
            "Power: {:.8f}, area: {:.5f}".format(
                ipc, cpi, power, area
            )
        f.write(msg + '\n')
        info("generate report: {}".format(ppa_rpt))


def main():
    simulator = path = os.path.join(root, "temp", "gem5-{}".format(args.idx))
    if if_exist(simulator):
        for benchmark in os.listdir(simulator):
            sim = os.path.join(simulator, benchmark)
            ppa_rpt = os.path.join(sim, "ppa.rpt")
            if not if_exist(ppa_rpt):
                generate_report(sim, args.fast_forward)
        info("report for {} is generated.".format(simulator))
    else:
        warn("{} not found.".format(simulator))



if __name__ == '__main__':
    args = parse_args()
    root = os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        os.path.pardir
    )
    main()
