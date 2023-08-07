# Author: baichen.bai@alibaba-inc.com


import os
from typing import List
from utils.utils import assert_error, if_exist
from utils.exceptions import UnSupportedException
from funcs.sim.o3cpu.o3cpu_simulation import O3CPUSimulation
from funcs.design.o3cpu.o3cpu_design_space import parse_design_space


def simulation_for_design_set(
    simulator: O3CPUSimulation,
    candidate_design_set: str
):
    if_exist(candidate_design_set, strict=True, quiet=False)
    with open(candidate_design_set, 'r') as f:
        cnt = f.readlines()
        embedding_set = []
        for design in cnt:
            if design.startswith('#'):
                continue
            embedding_set.append([int(i) for i in \
                    design.strip().strip('(').strip(')').split(',')
                ]
            )
        for embedding in embedding_set:
            simulator.simulate(embedding)


def simulation_for_select_embedding(
    simulator: O3CPUSimulation,
    candidate_embedding: List[List[int]]
):
    for embedding in candidate_embedding:
        simulator.simulate(embedding)


def simulation_for_select_idx(
    simulator: O3CPUSimulation,
    candidate_idx: List[List[int]]
):
    if isinstance(candidate_idx, list):
        for idx in candidate_idx:
            simulator.simulate(
                simulator.o3cpu_design_space.idx_to_embedding(idx)
            )
    elif isinstance(candidate_idx, str):
        if if_exist(candidate_idx, strict=True, quiet=False):
            with open(candidate_idx, 'r') as f:
                candidate_idx = f.readlines()
                for idx in candidate_idx:
                    simulator.simulate(
                        simulator.o3cpu_design_space.idx_to_embedding(
                            int(idx)
                        )
                    )
    else:
        raise UnSupportedException("unknown type for candidate_idx.")


def simulation_for_idx_range(
    simulator: O3CPUSimulation,
    start_idx: int,
    end_idx: int
):
    for idx in range(start_idx, end_idx + 1):
        simulator.simulate(
            simulator.idx_to_embedding(idx)
        )


def simulation(configs: dict):
    # create the simulator
    simulator = O3CPUSimulation(
        design_space=parse_design_space(
            configs["design-space"]
        ),
        configs=configs["simulation"]
    )

    configs = configs["simulation"]
    if configs["simulator"]["candidate-design-set"]:
        simulation_for_design_set(
            simulator,
            configs["simulator"]["candidate-design-set"]
        )
    elif configs["simulator"]["candidate-embedding"]:
        simulation_for_select_embedding(
            simulator,
            configs["simulator"]["candidate-embedding"]
        )
    elif configs["simulator"]["candidate-idx"]:
        simulation_for_select_idx(
            simulator,
            configs["simulator"]["candidate-idx"]
        )
    else:
        assert configs["simulator"]["start-idx"] and \
            configs["simulator"]["end-idx"] and \
            configs["simulator"]["end-idx"] >= \
            configs["simulator"]["start-idx"], \
            assert_error("invalid YAML for index range.")
        simulation_for_idx_range(
            simulator,
            configs["simulator"]["start-idx"],
            configs["simulator"]["end-idx"]
        )
