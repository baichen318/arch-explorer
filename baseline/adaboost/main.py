# Author: baichen318@gmail.com

import os
import sys
adaboost_root = os.path.abspath(
    os.path.join(
        os.path.dirname(__file__)
    )
)
arch_explorer_root = os.path.abspath(
    os.path.join(
        adaboost_root,
        os.path.pardir,
        os.path.pardir
    )
)
os.environ["MKL_THREADING_LAYER"] = "GNU"
import re
import random
import numpy as np
from time import time
from datetime import datetime
from helper import parse_args
from adaboost import AdaBoostRT
from sklearn.tree import DecisionTreeRegressor
from sklearn.ensemble import AdaBoostRegressor
from sklearn.neural_network import MLPRegressor
# from decision_tree import DecisionTreeRegressor
from utils.utils import get_configs, info, mkdir, if_exist
from funcs.sim.o3cpu.o3cpu_simulation import O3CPUSimulation
from funcs.design.o3cpu.o3cpu_design_space import parse_design_space


random.seed(2022)
np.random.seed(2022)
simulation_count = 0
pat = re.compile(r"\d+\.\d+")


def generate_L(dataset: np.ndarray):
    L = []
    for data in dataset:
        vec = data[:-3].astype(int)
        L.append(design_space.embedding_to_idx(list(vec)))
    return L


def generate_P_from_U(k, L):
    P = []
    for i in range(k):
        idx = random.sample(range(design_space.size), k=1)[0]
        while idx in L:
            idx = random.sample(range(design_space.size), k=1)[0]
        P.append(idx)
    return P


def load_dataset(path):
    if_exist(path, strict=True, quiet=False)
    return np.loadtxt(path)
    # feature : 22
    # perf: -3
    # power: -2
    # area: -1
    # features = dataset[:, :22]
    # perf = dataset[:, -3]
    # power = dataset[:, -2]
    # area = dataset[:, -1]
    # return dataset


def create_mlp(hidden):
    return MLPRegressor(
        hidden_layer_sizes=(16, hidden),
        activation="logistic",
        solver="adam",
        alpha=0.0001,
        learning_rate="adaptive",
        learning_rate_init=0.001,
        max_iter=50000,
        momentum=0.5,
        early_stopping=True
    )


def train_adaboost_rt(dataset, hidden):
    sample = dataset.shape[0]
    model = []
    # max. iteration
    T = 10
    # weight distribution and error
    epsilon = np.zeros(T)
    beta = np.zeros(T)
    fit = np.zeros((T, sample, 3))
    w = np.ones((T, sample))
    w /= np.linalg.norm(w, ord=1, axis=1).reshape(T, 1)
    phi = 0.2

    for t in range(T):
        idx = np.random.choice(range(sample), replace=True, p=w[t])
        x = dataset[idx][:-3]
        y = dataset[idx][-3:]
        # weak learner
        model.append(create_mlp(hidden).fit(
                np.expand_dims(x, axis=0),
                np.expand_dims(y, axis=0)
            )
        )
        # getback to hypothesis
        y_fit = model[t].predict(dataset[:, :-3])
        fit[t] = y_fit
        # adjusted error for each instance (PPA)
        ARE = np.mean(
            abs((dataset[:, -3:] - y_fit) / dataset[:, -3:]),
            axis=1
        )
        # calculate error of hypothesis
        epsilon[t] = np.sum(w[t][ARE > phi])
        # calculate beta
        beta[t] = epsilon[t] ** 2
        # update weight vector
        w[t + 1][ARE <= phi] = w[t][ARE <= phi] * beta[t]
        w[t + 1][ARE > phi] = w[t + 1][ARE > phi]
        # normalize
        w[t + 1] = w[t + 1] / np.sum(w[t + 1])

    # final model
    ww = np.log(1 / beta[1: -1])
    ret = ww * fit[1:T] / np.sum(ww)


def pseudo_train_adaboost_rt(dataset, hidden):
    T = 10
    model = []
    for i in range(T):
        # _model = AdaBoostRT(max_iter=1, estimator=DecisionTreeRegressor(max_depth=10))
        _model = AdaBoostRegressor(create_mlp(hidden), n_estimators=100)
        _model.fit(dataset[:, :-3], dataset[:, -3])
        model.append(_model)
    return model


def predict_adaboost_rt(adaboost_model, x):
    y = []
    for model in adaboost_model:
        y.append(model.predict(x))
    return np.array(y)


def calc_cv(data1: np.ndarray, data2: np.ndarray):
    """
        data1: prediction given by the first RT
        data2: prediction given by the second RT
    """
    t1 = data1.shape[0]
    t2 = data2.shape[0]
    y = np.concatenate((data1, data2), axis=0)

    # mu
    mu = np.sum(y, axis=0) / (t1 + t2)
    sigma = np.sqrt(np.sum(((y - mu)**2),axis=0))
    cv = sigma / mu
    cv = cv.tolist()
    # we sort `cv` from large -> small, and record the index
    # some of `cv` value can be the same
    sorted_list = []
    for i in range(len(cv)):
        sorted_list.append((i, cv[i]))
    # big -> small
    sorted_list = sorted(sorted_list, key=lambda x: x[1], reverse=True)

    sorted_index = []
    for i in range(len(sorted_list)):
        sorted_index.append(sorted_list[i][0])
    return sorted_index


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


def main():
    global simulation_count

    dataset = load_dataset(
        os.path.join(
            arch_explorer_root,
            "baseline",
            "adaboost",
            configs["dataset"]["output"]
        )
    )

    simulator = O3CPUSimulation(
        design_space=design_space,
        configs=configs["simulation"]
    )

    L = generate_L(dataset)

    # create a pool P by moving p unlabeled samples from U to P randomly
    P = generate_P_from_U(64, L)

    # use L to train Adaboost.RT H1 & H2 with M1 & M2 hidden neurons
    H1 = pseudo_train_adaboost_rt(dataset, 6)
    H2 = pseudo_train_adaboost_rt(dataset, 8)

    K = settings["K"]
    W = settings["W"]
    N = settings["N"]

    solution_set = []

    for i in range(K):
        info("process: {}/{}".format(i, K))
        log_file.write("process: {}/{}\n".format(i, K))
        # both H1 and H2 predict the P
        p = []
        for idx in P:
            p.append(design_space.idx_to_embedding(idx))
        p = np.array(p)
        y1 = predict_adaboost_rt(H1, p)
        y2 = predict_adaboost_rt(H2, p)

        # calculate c.v of each unlabeled sample in P and sort them
        cv = calc_cv(y1, y2)

        # choose N from top W randomly
        idx = random.sample(range(W), N)
        temp = []
        for j in idx:
            temp.append(P[cv[j]])
        # simulate the N samples
        for n in temp:
            solution_set.append(n)
            vec = design_space.idx_to_embedding(n)
            if need_simulate(simulator, n):
                simulator.simulate(vec)
            simulation_count += 1
            ipc, cpi, power, area = get_simulation_results(simulator, n)
            log_file.write(
                "simulation count: {} " \
                "embedding: {} idx: {} " \
                "ipc: {} cpi: {} power: {} area: {}\n".format(
                    simulation_count, vec, n, ipc, cpi, power, area
                )
            )
            log_file.flush()
            # move the newly labeled samples from P to L
            P.remove(n)
            dataset = np.insert(
                dataset,
                dataset.shape[0],
                np.concatenate((vec, [ipc, power, area])),
                axis=0
            )
            L.append(n)

        # rebuild H1, H2 by the new set L
        H1 = pseudo_train_adaboost_rt(dataset, 6)
        H2 = pseudo_train_adaboost_rt(dataset, 8)

        # replenish the P by choosing N examples from U at random
        _P = generate_P_from_U(N, L)
        for p in _P:
            P.append(p)

    mkdir(settings["report"]["path"])
    with open(os.path.join(settings["report"]["path"], "report"), 'w') as f:
        f.write("obtained solution: {}.\n".format(len(solution_set)))
        msg = ""
        for solution in solution_set:
            ipc, cpi, power, area = get_simulation_results(simulator, solution)
            msg += "{} {} ppa: {} {} {}\n".format(
                design_space.idx_to_embedding(solution), solution, ipc, power, area
            )
        f.write("{}\n".format(msg))
        f.write("simulation count: {}\n".format(simulation_count))
        f.write("\n")


if __name__ == "__main__":
    argv = parse_args()
    configs = get_configs(argv.configs)
    settings = get_configs(argv.settings)
    design_space = parse_design_space(
        configs["design-space"]
    )
    mkdir(settings["report"]["path"])
    log_file = open(os.path.join(
        settings["report"]["path"], "log"), 'w'
    )
    main()
