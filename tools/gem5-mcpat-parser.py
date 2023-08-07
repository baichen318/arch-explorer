# Author: baichen.bai@alibaba-inc.com


import os
import re
import json
import argparse
from xml.dom import minidom
from xml.etree import ElementTree as ET
from utils.utils import info, warn, error, assert_error


def create_parser():
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description="Gem5 to McPAT Parser"
    )
    parser.add_argument(
        "-c",
        "--configs",
        type=str,
        required=True,
        metavar="PATH",
        help="config.json of Gem5 outputs specification."
    )
    parser.add_argument(
        "-s",
        "--stats",
        type=str,
        required=True,
        metavar="PATH",
        help="stats.txt of Gem5 outputs specification."
    )
    parser.add_argument(
        "-t",
        "--template",
        type=str,
        required=True,
        metavar="PATH",
        help="template XML specification."
    )
    parser.add_argument(
        "-o",
        "--output",
        type=str,
        default="mcpat-in.xml",
        metavar="PATH",
        help="output XML specification."
    )
    return parser


def parse_xml(source):
    return ET.parse(source)


def read_configs(fconfigs):
    global configs
    with open(fconfigs, 'r') as f:
        configs = json.load(f)


def read_stats(fstats):
    global stats
    with open(fstats, 'r') as f:
        ignores = re.compile(r"^---|^$")
        stat_line = re.compile(r"([a-zA-Z0-9_\.:-]+)\s+([-+]?[0-9]+\.[0-9]+|[-+]?[0-9]+|nan|inf)")
        count = 0
        for line in f:
            # ignore empty lines and lines starting with "---"
            if not ignores.match(line):
                count += 1
                key = value = "nan"
                try:
                    key = stat_line.match(line).group(1)
                    value = stat_line.match(line).group(2)
                except AttributeError as e:
                    warn("generated stats.txt do not follow the pre-defined format.")
                if value == "nan":
                    warn("{} is \"nan\". set it to 0.".format(key))
                    value = '0'
                if key != "nan":
                    stats[key] = value


def read_mcpat_template(template):
    global mcpat_template
    global switch_mode
    if "switch" in template:
        switch_mode = True
    mcpat_template = parse_xml(template)


def get_num_cores():
    if switch_mode:
        return len(configs["system"]["switch_cpus"])
    return len(configs["system"]["cpu"])


def if_l2_exists():
    if switch_mode:
        return "l2cache" in configs["system"]["switch_cpus"][0]
    return "l2cache" in configs["system"]["cpu"][0]


def get_shared_l2():
    return "l2" in configs["system"]


def get_private_l2():
    if get_shared_l2():
        return '0'
    else:
        if if_l2_exists():
            return '1'
        else:
            return '0'


def get_num_of_l2s():
    if if_l2_exists():
        return get_num_cores()
    # NOTICE: McPAT assumes that a core must has a L2C
    # return '1' if get_shared_l2() else '0'
    return '1'


def get_isa(idx):
    return '1' \
        if configs["system"]["cpu"][idx]["isa"][0]["type"] == "X86ISA" else '0'


def if_cpu_stats_from_template(child):
    value = child.attrib.get("value")
    if value is not None and \
        "cpu." in value and \
        value.split('.')[0] == "stats":
        return child
    return None


def if_cpu_config_from_template(child):
    value = child.attrib.get("value")
    if value is not None and "config" in value.split('.')[0]:
        if "switch_cpus." in value:
            return child
        if "cpu."in value:
            return child
    return None


def replace_stats_for_multicore(value, idx):
    if "switch_cpus" in value:
        return value.replace("cpu.", "switch_cpus.{}.".format(idx))
    return value.replace("cpu.", "cpu.{}.".format(idx))


def replace_config_for_multicore(value, idx):
    if "switch_cpus" in value:
        return value.replace("switch_cpus.", "switch_cpus.{}.".format(idx))
    return value.replace("cpu.", "cpu.{}.".format(idx))


def multicore_extension(root):
    num_cores = get_num_cores()
    for idx in range(num_cores):
        root.attrib["name"] = "core.{}".format(idx)
        root.attrib["id"] = "system.core.{}".format(idx)
        for child in root:
            # traverse components at the "core" hierarchy
            _id = child.attrib.get("id")
            name = child.attrib.get("name")
            value = child.attrib.get("value")
            if name == "x86":
                value = get_isa(idx)
            if _id and "core" in _id:
                _id = _id.replace(
                    "core",
                    "core.{}".format(idx)
                )
            if num_cores > 1 and if_cpu_stats_from_template(child) is not None:
                value = replace_stats_for_multicore(value, idx)
            if if_cpu_config_from_template(child) is not None:
                value = replace_config_for_multicore(value, idx)
            if len(list(child)) != 0:
                for _child in child:
                    _value = _child.attrib.get("value")
                    if num_cores > 1 and if_cpu_stats_from_template(_child) is not None:
                        _value = replace_stats_for_multicore(_value, idx)
                    if if_cpu_config_from_template(_child) is not None:
                        _value = replace_config_for_multicore(_value, idx)
                    _child.attrib["value"] = _value
            if _id:
                child.attrib["id"] = _id
            if value:
                child.attrib["value"] = value


def l2_extension(root):
    num_cores = get_num_cores()
    for idx in range(num_cores):
        root.attrib["id"] = "system.L2.{}".format(idx)
        root.attrib["name"] = "L2.{}".format(idx)
        for child in root:
            value = child.attrib.get("value")
            if if_cpu_stats_from_template(child) is not None:
                value = replace_stats_for_multicore(value, idx)
            if if_cpu_config_from_template(child) is not None:
                value = replace_config_for_multicore(value, idx)
            if value:
                child.attrib["value"] = value


def misc_components_extension(root):
    num_cores = get_num_cores()
    if num_cores > 1:
        child = if_cpu_stats_from_template(root)
        if child is not None:
            value = child.attrib.get("value")
            if "switch_cpus" in value:
                value = "({})".format(value.replace("switch_cpus.", "switch_cpus.0."))
            else:
                value = "({})".format(value.replace("cpu.", "cpu.0."))
            for i in range(1, num_cores):
                if "switch_cpus" in value:
                    value = "{} + ({})".format(
                        value,
                        value.replace("cpu.", "switch_cpus.{}.".format(i))
                    )
                else:
                    value = "{} + ({})".format(
                        value,
                        value.replace("cpu.", "cpu.{}.".format(i))
                    )
            child.attrib["value"] = value


def generate_mcpat_xml():
    def format_xml(root):
        rough_string = ET.tostring(root, "utf-8")
        reparsed = minidom.parseString(rough_string)
        reparsed.toprettyxml(indent='\t')

    root = mcpat_template.getroot()
    num_cores = get_num_cores()
    for child in root[0]:
        # traverse components at the "system" hierarchy
        name = child.attrib.get("name")
        if name == "number_of_cores":
            child.attrib["value"] = str(num_cores)
        if name == "number_of_L2s":
            child.attrib["value"] = get_num_of_l2s()
        if name == "Private_L2":
            child.attrib["value"] = get_private_l2()
        if name == "core":
            multicore_extension(child)
        if name == "L2":
            if get_private_l2():
                l2_extension(child)
        # other components may be specified explicitly
        misc_components_extension(child)
    format_xml(root)


def get_config(conf):
    split_conf = re.split(r"\.", conf)
    curr_conf = configs
    for x in split_conf:
        if x.isdigit():
            assert isinstance(curr_conf, list), \
                assert_error("{} does not exist in gem5 config.".format(conf))
            # print(curr_conf)
            assert len(curr_conf) > int(x), \
                assert_error("{} in {} does not exist in gem5 config.".format(int(x), conf))
            curr_conf = curr_conf[int(x)]
        elif x in curr_conf:
            curr_conf = curr_conf[x]
        else:
            warn("{} in {} does not exist in gem5 config.".format(x, conf))
    return curr_conf if curr_conf != None else 0


def write_mcpat_xml(output_path):
    root = mcpat_template.getroot()
    pattern = re.compile(r"config\.([][a-zA-Z0-9_:\.]+)")
    # replace params with values from the GEM5 config file
    for param in root.iter("param"):
        name = param.attrib["name"]
        value = param.attrib["value"]
        if "config" in value:
            # param
            all_confs = pattern.findall(value)
            for conf in all_confs:
                conf_value = get_config(conf)
                if isinstance(conf_value, dict) or isinstance(conf_value, list):
                    conf_value = 0
                    warn("{} does not exist in gem5 config.".format(conf))
                value = re.sub("config." + conf, str(conf_value), value)
            if ',' in value:
                # e.g., pipelines_per_core, pipeline_depth
                exprs = re.split(',', value)
                for i in range(len(exprs)):
                    exprs[i] = str(eval(exprs[i]))
                param.attrib["value"] = ','.join(exprs)
            else:
                param.attrib["value"] = str(eval(str(value)))
    # replace stats with values from the GEM5 stats file
    stat_pattern = re.compile(r"stats\.([a-zA-Z0-9_:\.]+)")
    for stat in root.iter("stat"):
        name = stat.attrib["name"]
        value = stat.attrib["value"]
        if "stats" in value:
            all_stats = stat_pattern.findall(value)
            expr = value
            for i in range(len(all_stats)):
                if all_stats[i] in stats:
                    expr = re.sub(
                        "stats.{}".format(all_stats[i]),
                        stats[all_stats[i]],
                        expr,
                        count=1
                    )
                else:
                    expr = re.sub(
                        "stats.{}".format(all_stats[i]),
                        str(0),
                        expr,
                        count=1
                    )
                    warn("{} does not exist in gem5 stats.".format(all_stats[i]))
            if "config" not in expr and "stats" not in expr:
                try:
                    stat.attrib["value"] = str(eval(expr))
                except ZeroDivisionError as e:
                    error("{}".format(e))
    # write out the xml file.
    with open(output_path, 'wb') as f:
        mcpat_template.write(f)
    info("create McPAT input xml: {}".format(output_path))


def main():
    read_configs(args.configs)
    read_stats(args.stats)
    read_mcpat_template(args.template)
    generate_mcpat_xml()
    write_mcpat_xml(args.output)


if __name__ == "__main__":
    args = create_parser().parse_args()
    configs = None
    stats = {}
    switch_mode = False
    mcpat_template = None
    main()
