# Author: baichen318@gmail.com


import os
import sys
arch_ranker_root = os.path.abspath(
    os.path.join(
        os.path.dirname(__file__)
    )
)
arch_explorer_root = os.path.abspath(
    os.path.join(
        arch_ranker_root,
        os.path.pardir,
        os.path.pardir
    )
)
import re
import random
import argparse
import itertools
import numpy as np
from helper import parse_args
from rankboost import BipartiteRankBoost
from funcs.sim.o3cpu.o3cpu_simulation import O3CPUSimulation
from funcs.design.o3cpu.o3cpu_design_space import parse_design_space
from utils.utils import if_exist, info, remove, mkdir, get_configs, assert_error


random.seed(2023)
np.random.seed(2023)
simulation_count = 0
pat = re.compile(r"\d+\.\d+")
explored_set = []


def load_dataset(path):
    if_exist(path, strict=True, quiet=False)
    dataset = np.loadtxt(path)
    features = dataset[:, :22]
    perf = dataset[:, -3]
    power = dataset[:, -2]
    area = dataset[:, -1]
    return features, perf, power, area


def create_pairs(X, y):
    X_pairs = []
    y_pairs = []

    for i in range(len(y)):
        for j in range(len(y)):
            if j > i:
                X_pairs.append(X[i] - X[j])
                if y[i] - y[j] > 1e-6:
                    y_pairs.append(1)
                else:
                    y_pairs.append(0)
    return X_pairs, y_pairs


def rank_boost(X, y):
    kendall_tau = 0.0
    mape_final = 0.0
    spearman_tau = 0.0
    X_pairs, y_pairs = create_pairs(X, y)
    X_pairs, y_pairs = np.array(X_pairs), np.array(y_pairs)
    model = BipartiteRankBoost()
    model.fit(X_pairs, y_pairs)
    return model


def get_simulation_results(simulator, idx):
    ipc, cpi, power, area = [], [], [], []
    for k, v in simulator.benchmark.macros.items():
        rpt = os.path.join(
            arch_explorer_root,
            "temp",
            "gem5-{}".format(idx),
            k,
            "ppa.rpt"
        )
        if if_exist(rpt, quiet=False):
            with open(rpt, 'r') as fin:
                result = fin.read()
                result = re.findall(pat, result)
                _ipc = float(result[0])
                _cpi = float(result[1])
                _power = float(result[2])
                _area = float(result[3])
                ipc.append(_ipc)
                cpi.append(_cpi)
                power.append(_power)
                area.append(_area)
    if len(ipc) == 0:
        return 0, 0, 0, 0
    ipc = np.average(ipc)
    cpi = np.average(cpi)
    power = np.average(power)
    area = np.average(area)
    return ipc, cpi, power, area


def need_simulate(simulator, idx) -> bool:
    simulator_root = os.path.join(
        arch_explorer_root,
        "temp",
        "gem5-{}".format(idx)
    )
    if if_exist(simulator_root):
        for k, v in simulator.benchmark.macros.items():
            ppa_rpt = os.path.join(simulator_root, k, "ppa.rpt")
            if not if_exist(ppa_rpt):
                return True
        return False
    return True


def binary_search_for_power_and_area(design_space, simulator, rank_list, metric):
    global simulation_count
    global explored_set

    low = 0
    high = len(rank_list) - 1
    mid = 0
 
    while low <= high:
 
        mid = (high + low) // 2

        if mid == 0:
            return rank_list[mid]

        _idx = rank_list[mid]
        embedding = design_space[_idx].tolist()
        idx = simulator.o3cpu_design_space.embedding_to_idx(embedding)
        if need_simulate(simulator, idx):
            simulator.simulate(embedding)

        simulation_count += 1
        explored_set.append(idx)

        ipc, _, power, area = get_simulation_results(simulator, idx)
        if metric == "power":
            val = power
        elif metric == "area":
            val = area

        if val <= threshold[metric]:
            return rank_list[:mid + 1]
 
        elif val > threshold[metric]:
            high = mid - 1


def binary_search_for_perf(design_space, simulator, rank_list, metric):
    global simulation_count
    global explored_set

    low = 0
    high = len(rank_list) - 1
    mid = 0
 
    while low <= high:
 
        mid = (high + low) // 2

        if mid == high:
            return rank_list[mid]

        _idx = rank_list[mid]
        embedding = design_space[_idx].tolist()
        idx = simulator.o3cpu_design_space.embedding_to_idx(embedding)
        if need_simulate(simulator, idx):
            simulator.simulate(embedding)

        simulation_count += 1
        explored_set.append(idx)

        ipc, _, power, area = get_simulation_results(simulator, idx)

        if ipc < threshold[metric]:
            low = mid + 1
 
        elif ipc >= threshold[metric]:
            return rank_list[mid:]


def prune_design_space(design_space, simulator, model, metric):
    global simulation_count
    global explored_set

    if design_space.ndim == 1:
        return [0]

    n_samples = design_space.shape[0]

    info("constructing pairs for {}.".format(metric))
    # small -> large
    rank_list = []

    # comb = itertools.combinations(range(n_samples), 2)
    # for k, (i, j) in enumerate(comb):
    #     rank_list[i, j] = model.predict_proba(
    #         np.expand_dims(design_space[i] - design_space[j], axis=0)
    #     )

    # NOTICE: the partial rankings cannot induce a full ranking list
    # this is an undiscussed problem in ArchRanker
    # we generate the ranking list w.r.t. the following method
    for i in range(n_samples):
        if i % 100 == 0:
            info("progress: {}/{} for {}".format(i + 1, n_samples, metric))
        if len(rank_list) == 0:
            rank_list.append(i)
            continue
        prev_len = len(rank_list)
        for j in rank_list:
            feature = np.expand_dims(design_space[i] - design_space[j], axis=0)
            pred = model.predict_proba(feature)
            # we sort following small -> large, hence, we find the first element
            # in `rank_list` that is larger than `i`
            if pred <= 0.5:
                # i < j
                rank_list.insert(rank_list.index(j), i)
                break
        if prev_len == len(rank_list):
            rank_list.append(i)
    assert len(rank_list) == n_samples, \
        assert_error("rank_list: {}, n_samples: {}".format(rank_list, n_samples))

    info("binary search for {}.".format(metric))

    if metric == "power" or metric == "area":
        rank_list = binary_search_for_power_and_area(
            design_space, simulator, rank_list, metric
        )
    elif metric == "perf":
        rank_list = binary_search_for_perf(
            design_space, simulator, rank_list, metric
        )
    return rank_list


def main():
    global simulation_count
    global explored_set

    simulator = O3CPUSimulation(
        design_space=parse_design_space(
            configs["design-space"]
        ),
        configs=configs["simulation"],
    )

    idx = random.sample(
        range(1, simulator.o3cpu_design_space.size + 1),
        k=settings["sample-size"]
    )
    design_space = []
    for i in idx:
        design_space.append(
            simulator.o3cpu_design_space.idx_to_embedding(i)
        )
    design_space = np.array(design_space)

    # load the dataset
    feature, perf, power, area = load_dataset(
        os.path.join(
            arch_explorer_root,
            "baseline",
            "archranker",
            configs["dataset"]["output"]
        )
    )

    # area
    info("construct rank boost for area")
    area_model = rank_boost(feature, area)
    design_space = design_space[prune_design_space(design_space, simulator, area_model, "area")]
    if design_space.ndim == 1:
        design_space = design_space[np.newaxis, :]
    info("simulator count for area: {}".format(simulation_count))

    # power
    power_model = rank_boost(feature, power)
    info("construct rank boost for power")
    design_space = design_space[prune_design_space(design_space, simulator, power_model, "power")]
    if design_space.ndim == 1:
        design_space = design_space[np.newaxis, :]
    info("simulator count for power: {}".format(simulation_count))

    # perf
    perf_model = rank_boost(feature, perf)
    info("construct rank boost for performance")
    design_space = design_space[prune_design_space(design_space, simulator, perf_model, "perf")]
    if design_space.ndim == 1:
        design_space = design_space[np.newaxis, :]
    info("simulator count for performance: {}".format(simulation_count))

    if design_space.ndim == 1:
        design_space = design_space[np.newaxis, :]
    info("find solutions: {}".format(design_space.shape[0]))
    for design in design_space:
        if need_simulate(
            simulator,
            simulator.o3cpu_design_space.embedding_to_idx(design.tolist())
        ):
            simulator.simulate(design.tolist())

    # report
    mkdir(settings["report"]["path"])
    remove(os.path.join(settings["report"]["path"], "report"))
    assert simulation_count == len(explored_set), \
        assert_error("simulation count: {}, explored set: {}".format(
            simulation_count, explored_set)
        )
    with open(os.path.join(settings["report"]["path"], "report"), 'w') as fout:
        msg = "simulation count: {}\n".format(simulation_count)
        # record the searched designs
        for i in range(simulation_count):
            ipc, cpi, power, area = get_simulation_results(simulator, explored_set[i])
            embedding = simulator.o3cpu_design_space.idx_to_embedding(explored_set[i])
            msg += "{} simulation embedding: {} idx: {} " \
                "ipc: {} cpi: {} power: {} area: {}\n".format(
                i + 1, embedding, explored_set[i], ipc, cpi, power, area
            )
        # record the solutions
        n_samples = design_space.shape[0]
        msg += "\n\nsolutions: {}\n".format(n_samples)
        for i in range(n_samples):
            idx = simulator.o3cpu_design_space.embedding_to_idx(design_space[i].tolist())
            ipc, cpi, power, area = get_simulation_results(simulator, idx)
            msg += "{} embedding: {} idx: {} " \
                "ipc: {} cpi: {} power: {} area: {}\n".format(
                i + 1, design_space[i].tolist(), idx, ipc, cpi, power, area
            )
        fout.write(msg)
        info("done")


if __name__ == "__main__":
    argv = parse_args()
    configs = get_configs(argv.configs)
    settings = get_configs(argv.settings)
    threshold = {
        "perf": 0.75,
        "power": 0.50,
        "area": 5.5
    }
    main()
