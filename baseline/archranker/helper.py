# Author: baichen.bai@alibaba-inc.com


import argparse


def parse_args():
    def initialize_parser(parser):
        parser.add_argument(
        	"-c",
        	"--configs",
            required=True,
            type=str,
            default="configs.yml",
            help="YAML file to be handled")
       	parser.add_argument(
        	"-s",
        	"--settings",
            required=True,
            type=str,
            default="settings.yml",
            help="YAML file to be handled")
        return parser

    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser = initialize_parser(parser)
    return parser.parse_args()