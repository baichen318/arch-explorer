# Author: baichen.bai@alibaba-inc.com


import os
from funcs.initialize import initialize
from algo.dse import archexplorer
from funcs.simulation import simulation
from utils.utils import get_configs_from_command
from funcs.dataset_generation import dataset_generation


def main(configs):
    if configs["mode"].startswith("initialize"):
        initialize(configs)
    elif configs["mode"].startswith("simulation"):
        simulation(configs)
    elif configs["mode"].startswith("dataset-generation"):
        dataset_generation(configs)
    elif configs["mode"].startswith("exploration"):
        archexplorer(configs)
    else:
        raise NotImplementedError()


if __name__ == "__main__":
    main(get_configs_from_command())
