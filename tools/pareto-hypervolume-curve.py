# Author: baichen.bai@alibaba-inc.com


import os
import sys
import torch
import argparse
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from utils.utils import if_exist, info, mkdir
from botorch.utils.multi_objective.pareto import is_non_dominated
from botorch.utils.multi_objective.hypervolume import Hypervolume



"""
    Calculate the Pareto frontier w.r.t. each simulation round.
    The input format:

    IPC Power   Area
    1.446436 0.277000 4.667540
    1.384722 0.241370 4.203000
    1.385652 0.243930 4.347120
    1.104443 0.169887 3.534740
    0.621078 0.066032 2.391610
    0.779365 0.090412 2.488810
    0.800599 0.093258 2.570440
    0.786146 0.092069 2.498540
    0.622765 0.066767 2.397400
    0.616425 0.065983 2.331150
    0.808389 0.094772 2.579850
    1.329852 0.233875 3.700440
    0.615044 0.065558 2.328970
"""


markers = [
    '.', ',', 'o', 'v', '^', '<', '>', '1', '2', '3',
    '4', '8', 's', 'p', 'P', '*', 'h', 'H', '+', 'x',
    'X', 'D', 'd', '|', '_'
]
colors = [
    'c', 'b', 'g', 'r', 'm', 'y', 'k', # 'w'
]
ref_point = torch.Tensor([0, -1.0, -15.0])


def create_parser():
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description="Calculate Pareto Hypervolume"
    )
    parser.add_argument(
        "-s",
        "--solution",
        type=str,
        required=True,
        metavar="PATH",
        help="solution specification."
    )
    parser.add_argument(
        "-o",
        "--output",
        type=str,
        required=False,
        default="output",
        metavar="PATH",
        help="solution directory specification."
    )
    return parser


def get_pareto_frontier(y: torch.Tensor):
    """
        NOTICE: `is_non_dominated` assumes maximization
    """
    return y[is_non_dominated(y)]


def calc_pareto_hypervolume(ppa: np.ndarray, vis: bool = False):
    tppa = torch.Tensor(ppa)

    # inverse the data
    tppa[:, 1] = -tppa[:, 1]
    tppa[:, 2] = -tppa[:, 2]

    # get the Pareto frontier
    pareto_frontier = get_pareto_frontier(tppa).numpy()

    # calculate Pareto hypervolume
    hv = Hypervolume(ref_point=ref_point)
    pareto_hypervolume = hv.compute(
        torch.from_numpy(pareto_frontier)
    )

    # recover the data
    pareto_frontier[:, 1] = -pareto_frontier[:, 1]
    pareto_frontier[:, 2] = -pareto_frontier[:, 2]

    if vis:
        fig = plt.figure()
        ax = fig.add_subplot(projection='3d')
        ax.scatter(
            ppa[:, 0],
            ppa[:, 1],
            ppa[:, 2],
            s=3,
            marker=markers[19],
            c=colors[-1],
            alpha=0.2,
            label="Data"
        )
        ax.scatter(
            pareto_frontier[:, 0],
            pareto_frontier[:, 1],
            pareto_frontier[:, 2],
            s=3,
            marker=markers[12],
            c=colors[1],
            label="Pareto"
        )
        plt.legend()
        plt.xlabel("IPC")
        plt.ylabel("Power")
        plt.title("Perf vs. Power")
        plt.show()

    pareto_frontier_saved_report = os.path.join(
        args.output,
        "pareto-frontier.rpt"
    )
    np.savetxt(
        pareto_frontier_saved_report,
        pareto_frontier,
        fmt="%f"
    )
    info("saving {}.".format(
        pareto_frontier_saved_report)
    )
    info("Pareto hypervolume: {}".format(
        pareto_hypervolume)
    )
    return pareto_hypervolume


def main():
    if_exist(args.solution, strict=True)
    mkdir(args.output)
    solution = pd.read_csv(args.solution, sep="\\s+")
    ppa = solution.values

    # we slide `ppa` to draw Pareto hypervolume curves
    n_designs = ppa.shape[0]
    hv = []
    for i in range(1, n_designs + 1):
        hv.append(calc_pareto_hypervolume(ppa[:i]))
    hv = np.array(hv)
    pareto_hy_curve = os.path.join(
        args.output, "pareto-curve.rpt"
    )
    np.savetxt(pareto_hy_curve, hv, fmt="%f")
    info("saving {}.".format(pareto_hy_curve))


if __name__ == "__main__":
    args = create_parser().parse_args()
    main()
