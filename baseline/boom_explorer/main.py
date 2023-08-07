# Author: baichen318@gmail.com


import os
import sys
from aux.aux import get_configs, parse_args_v2
from baseline.boom_explorer.algo.boom_explorer import boom_explorer


def main():
    boom_explorer(configs, settings)


if __name__ == "__main__":
    argv = parse_args_v2()
    configs = get_configs(argv.configs)
    settings = get_configs(argv.settings)
    main()
