# Author: baichen.bai@alibaba-inc.com


import os
import argparse
from algo.core.model import Graph
from algo.core.instruction import RiscvInstructionStream
from utils.utils import get_configs_from_command, info, Timer


def parse_args():
    def initialize_parser(parser):
        parser.add_argument(
            "-t", "--trace",
            required=True,
            type=str,
            default="trace",
            help="trace file specification")
        parser.add_argument(
            "-o", "--output",
            required=True,
            type=str,
            default="report",
            help="report specification"
        )
        parser.add_argument(
            "-v", "--view",
            action="store_true",
            default=False,
            help="view the new DEG"
        )
        parser.add_argument(
            "-s", "--start",
            type=int,
            default=-1,
            help="instruction start index"
        )
        parser.add_argument(
            "-e", "--end",
            type=int,
            default=-1,
            help="instruction end intex"
        )
        return parser

    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser = initialize_parser(parser)
    return parser.parse_args()


def construct_new_graph_formulation(configs, trace):
    graph = Graph()

    with Timer("construct new DEG"):
        for inst in trace:
            if (inst.seq + 1) % min(len(trace), 10000) == 0:
                info("reading the instruction: {}/{}.".format(
                        inst.seq + 1,
                        len(trace)
                    )
                )
            graph.model(inst)
            graph.model_interaction(inst)

    # graph.construct_critical_path_v1()
    graph.construct_critical_path_v2()
    graph.generate_report(configs.output)
    info("trace: {}, graph nodes: {}, graph edges: {}".format(
            trace.benchmark, len(graph.graph.nodes), len(graph.graph.edges)
        )
    )
    if configs.view:
        basename = os.path.basename(configs.output)
        pdf = os.path.join(
            os.path.dirname(configs.output),
            "{}".format(os.path.splitext(basename)[0])
        )
        graph.view(pdf, configs.start, configs.end)


def main(configs):
	trace = RiscvInstructionStream(configs.trace)
	construct_new_graph_formulation(configs, trace)


if __name__ == "__main__":
	main(parse_args())
