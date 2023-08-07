# Author: baichen318@gmail.com


import os
import sys
calipers_root = os.path.abspath(
    os.path.join(
        os.path.dirname(__file__)
    )
)
arch_explorer_root = os.path.abspath(
    os.path.join(
        calipers_root,
        os.path.pardir,
        os.path.pardir
    )
)
import re
import random
import argparse
import itertools
import numpy as np
from typing import List
from multiprocessing.pool import ThreadPool
from funcs.sim.o3cpu.o3cpu_simulation import O3CPUSimulation
from funcs.design.o3cpu.o3cpu_design_space import parse_design_space
from utils.utils import if_exist, info, remove, parse_args, mkdir, get_configs, \
    execute, assert_error, remove_suffix



calipers_bin = os.path.join(
    arch_explorer_root,
    "infras",
    "calipers",
    "build",
    "calipers"
)


def calipers(simulator: object, idx: int, embedding: List[int]):
    temp = os.path.join(calipers_root, "temp")
    mkdir(temp)
    config_file = os.path.join(temp, str(idx))
    # generate the configuration file
    with open(config_file, 'w') as f:
        msg = """
ISA                      RISC-V
Core                     OoO
Branch_Predictor         TraceB
I_Cache                  TraceC
D_Cache                  TraceC
Instr_Buffer_Size        {}
Instr_Queue_Size         {}
Fetch_Bandwidth          {}
Dispatch_Bandwidth       {}
Issue_Bandwidth          {}
Commit_Bandwidth         {}
Decode_Cycles            1
Dispatch_Cycles          2
Execute_To_Commit_Cycles 3
Prediction_Cycles        0
Misprediction_Penalty    2
Mem_Issue_Bandwidth      2
Mem_Commit_Bandwidth     2
Int_ALU_Count            {}
Int_Mul_Div_Count        1
FP_ALU_Count             {}
FP_Mul_Div_Count         1
LSU_Count                1
LQ_Size                  {}
SQ_Size                  {}

        """.format(
            embedding[8], embedding[11], embedding[0], embedding[0], embedding[0], embedding[0],
            embedding[14], embedding[16], embedding[12], embedding[13]
        )
        f.write(msg)
    # simulation
    simulator.simulate(embedding, use_gantt=False)

    # call Calipers
    if "bare-model" in simulator.benchmark.keys():
        # bare-model
        l = len(simulator.benchmark["bare-model"])
        benchmark_names = [remove_suffix(i, ".riscv") for i in simulator.benchmark["bare-model"]]
    else:
        l = len(simulator.benchmark.keys())
        benchmark_names = simulator.benchmark.keys()
    pool = ThreadPool(l)
    for bmark in benchmark_names:
        trace = os.path.join(
            arch_explorer_root, "temp", "gem5-{}".format(idx), bmark, "instruction-flow"
        )
        output = os.path.join(
            arch_explorer_root, "temp", "gem5-{}".format(idx), bmark, "calipers.rpt"
        )
        if if_exist(trace):
            cmd = "{} -c {} -t {} -o {}".format(
                calipers_bin,
                config_file,
                trace,
                output
            )
            pool.apply_async(execute, (cmd,),)
    pool.close()
    pool.join()
    exit()


def main():
    if_exist(calipers_bin, strict=True, quiet=False)

    design_space = parse_design_space(configs["design-space"])
    simulator = O3CPUSimulation(
        design_space,
        configs["simulation"]["benchmark"],
        # os.path.join(configs["simulation"]["gem5-path"], configs["simulation"]["gem5-id"]),
        os.path.join(configs["simulation"]["gem5-path"]),
        vis=False
    )
    # we sweep the design space
    for i in range(1, design_space.size + 1):
        calipers(simulator, i, design_space.idx_to_embedding(i))



if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    configs = get_configs(parse_args().configs)
    main()
