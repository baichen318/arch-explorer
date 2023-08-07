# Author: baichen.bai@alibaba-inc.com


import os
import re
import sys
import platform
import multiprocessing
from collections import OrderedDict
from utils.thread import WorkerThread
from multiprocessing.pool import ThreadPool
from funcs.sim.base_simulation import Simulation
from funcs.sim.benchmark.spec2006 import construct_spec2006
from funcs.sim.benchmark.spec2017 import construct_spec2017
from funcs.sim.benchmark.bare_model import construct_bare_model
from funcs.design.o3cpu.o3cpu_design_space import O3CPUDesignSpace
from typing import List, Union, Tuple, NoReturn, Dict, Callable, Optional
from utils.utils import if_exist, mkdir, execute, remove_suffix , info, warn, \
    error, assert_error, Timer


def get_perf(manager: object, stats: str) -> Tuple[float, float]:
    ipc = cpi = 0
    if if_exist(stats):
        with open(stats, 'r') as f:
            cnt = f.readlines()
        """
            Use different CPUs based on the fast forwarding.
        """
        if manager.benchmark.name == "spec2017":
            """
                SPEC2017 uses fast forwarding.
            """
            cpu = "switch_cpus"
        elif manager.benchmark.name == "spec2006":
            """
                SPEC2006 uses fast forwarding.
            """
            cpu = "switch_cpus"
        else:
            """
                The bare model can use fast forwarding.
            """
            if manager.benchmark.warmup_insts is not None and \
                manager.benchmark.fast_forward is not None:
                cpu = "switch_cpus"
            else:
                cpu = "cpu"
        for line in cnt:
            if line.startswith("system.{}.ipc".format(cpu)):
                ipc = float(line.split()[1])
                continue
            elif line.startswith("system.{}.cpi".format(cpu)):
                cpi = float(line.split()[1])
    return ipc, cpi


def get_power_area(report: str) -> Tuple[float, float]:
    p_subthreshold = re.compile(r"Subthreshold\ Leakage\ =\ [+-]?(\d+(\.\d*)?|\.\d+)([eE][+-]?\d+)?\ W")
    p_gate = re.compile(r"Gate\ Leakage\ =\ [+-]?(\d+(\.\d*)?|\.\d+)([eE][+-]?\d+)?\ W")
    p_dynamic = re.compile(r"Runtime\ Dynamic\ =\ [+-]?(\d+(\.\d*)?|\.\d+)([eE][+-]?\d+)?\ W")
    p_area = re.compile(r"Area\ =\ [+-]?(\d+(\.\d*)?|\.\d+)([eE][+-]?\d+)?\ mm\^2")
    subthreshold, gate, dynamic, area = 0, 0, 0, 0
    if if_exist(report):
        with open(report, 'r') as f:
            cnt = f.read()
            try:
                subthreshold = float(
                    p_subthreshold.findall(cnt)[1][0]
                )
                gate = float(
                    p_gate.findall(cnt)[1][0]
                )
                dynamic = float(
                    p_dynamic.findall(cnt)[1][0]
                )
                area = float(
                    p_area.findall(cnt)[1][0]
                )
            except Exception as e:
                error("McPAT report {} is " \
                    "failed to generate.".format(report)
                )
        return subthreshold + gate + dynamic, area


def generate_report(manager: object, k: str) -> Tuple[float, float, float, float]:
    m5out = os.path.join(manager.temp, k)
    stats = os.path.join(m5out, "stats.txt")
    report = os.path.join(m5out, "report")
    ppa_rpt = os.path.join(m5out, "ppa.rpt")
    ipc, cpi = get_perf(manager, stats)
    power, area = get_power_area(report)
    with open(ppa_rpt, 'w') as f:
        msg = "IPC: {:.8f}, CPI: {:.8f}, " \
            "Power: {:.8f}, area: {:.5f}".format(
                ipc, cpi, power, area
            )
        f.write(msg + '\n')
    return ipc, cpi, power, area


def pat_model_impl(manager: object, k: str) -> NoReturn:
    """
        `k` is the benchmark's name.
    """
    if manager.benchmark.name == "spec2017":
        m5out = os.path.join(manager.temp, k)
    elif manager.benchmark.name == "spec2006":
        m5out = os.path.join(manager.temp, k)
    else:
        # bare model
        m5out = os.path.join(
            manager.temp,
            remove_suffix(k, ".riscv")
        )

    """
        We use the "switch-o3cpu.xml" template if
        we specify the fast forwarding.
    """
    if manager.benchmark.name == "spec2017":
        """
            SPEC2017 uses fast forwarding by default.
        """
        template = os.path.join(
            manager.macros["arch-explorer-root"],
            "tools",
            "templates",
            "switch-o3cpu.xml"
        )
    elif manager.benchmark.name == "spec2006":
        """
            SPEC2006 uses fast forwarding by default.
        """
        template = os.path.join(
            manager.macros["arch-explorer-root"],
            "tools",
            "templates",
            "switch-o3cpu.xml"
        )
    else:
        # bare model
        if manager.benchmark.warmup_insts is not None and \
            manager.benchmark.fast_forward is not None:
            """
                We use "switch-o3cpu.xml" due to the
                fast forwarding.
            """
            template = os.path.join(
                manager.macros["arch-explorer-root"],
                "tools",
                "templates",
                "switch-o3cpu.xml"
            )
        else:
            """
                We use "o3cpu.xml" due to no fast
                forwarding.
            """
            template = os.path.join(
                manager.macros["arch-explorer-root"],
                "tools",
                "templates",
                "o3cpu.xml"
            )

    cmd = "{}; {} {} -c {} " \
        "-s {} " \
        "-t {} " \
        "-o {}".format(
            manager.env_manager.generate_pyenv(),
            sys.executable,
            os.path.join(
                manager.macros["arch-explorer-root"],
                "tools",
                "gem5-mcpat-parser.py"
            ),
            os.path.join(m5out, "config.json"),
            os.path.join(m5out, "stats.txt"),
            template,
            os.path.join(m5out, "mcpat.xml")
        )
    cmd = "{} && {} -infile {} -print_level 5 > {}".format(
        cmd,
        os.path.join(
            manager.macros["mcpat-research-root"],
            "mcpat"
        ),
        os.path.join(m5out, "mcpat.xml"),
        os.path.join(m5out, "report")
    )
    """
        NOTICE: the power & area modeling could be failed.
    """
    execute(cmd)


def pat_model(manager: object, k: str) -> Tuple[float, float, float, float]:
    with Timer("PAT model with {}".format(k)):
        pat_model_impl(manager, k)
    return generate_report(manager, k)


def trim_instruction_flow(m5out: str):
    """
        We trim the trace via fast forwarded simulation.
    """
    f = os.path.join(m5out, "instruction-flow")
    with open(f, 'r') as fin:
        cnt = fin.readlines()
        i = 0
        for line in cnt:
            if "DST=" in line:
                break
            i += 1
        with open(f, 'w') as fout:
            fout.write(''.join(cnt[i:]))


def simulation_spec2017_impl(
    embedding: List[int], manager: object, k: str, v: Dict
) -> NoReturn:
    """
        `k` is the benchmark's name, and `v` is
        meta information of `k`.
    """
    # `m5out` is the target output directory
    m5out = os.path.join(manager.temp, k)

    # change the execution directory
    cmd = "cd {} && {} ".format(
        v["benchmark-root"],
        os.path.join(
            manager.macros["gem5-research-root"],
            "build", "RISCV", manager.gem5_opt
        )
    )

    if manager.configs["misc-setting"]["deg-model"]:
        """
            If we use the new DEG to model, we need
            to generate the trace.
        """
        cmd = "{} --debug-file=instruction-flow  " \
            "--debug-flags=Exec ".format(cmd)
    cmd = "{} " \
        "--outdir={} " \
        "{} " \
        "--num-cpus=1 " \
        "--cpu-type=RiscvO3CPU " \
        "--cmd={} ".format(
            cmd,
            os.path.join(
                manager.macros["gem5-research-root"], k
            ),
            os.path.join(
                manager.macros["gem5-research-root"],
                "configs", "example", "se.py"
            ),
            v["elf"]
        )
    # append benchmark's options/inputs
    if v["options"] is not None:
        if v["inputs"] is not None:
            cmd = "{} " \
                "--options=\"{} --inputs={}\"".format(
                    cmd,
                    v["options"],
                    v["inputs"]
                )
        else:
            cmd = "{} " \
                "--options=\"{}\" ".format(
                    cmd,
                    v["options"]
                )
    cmd = "{} " \
        "--caches " \
        "--cacheline_size=64 " \
        "--l1i_size={}kB " \
        "--l1i_assoc={} " \
        "--l1d_size={}kB " \
        "--l1d_assoc={} " \
        "--bp-type=TournamentBP " \
        "--sys-clock=1GHz " \
        "--cpu-clock=1GHz " \
        "--sys-voltage=0.63V " \
        "--mem-size=16GB " \
        "--mem-type=LPDDR3_1600_1x32 " \
        "--mem-channels=1 " \
        "--restore-simpoint-checkpoint -r {} " \
        "--checkpoint-dir {} ".format(
            cmd,
            embedding[18],
            embedding[19],
            embedding[20],
            embedding[21],
            v["checkpoint"],
            v["checkpoint-root"]
        )
    if manager.benchmark.max_insts is not None:
        cmd = "{} --maxinsts={}".format(
            cmd,
            manager.benchmark.max_insts
        )
    cmd = "{} && rm -rf {} && mv -f {} {}".format(
        cmd, m5out, os.path.join(
            manager.macros["gem5-research-root"], k
        ), m5out
    )

    # simulate
    with Timer("simulate with {}".format(cmd)):
        execute(cmd)

    if not if_exist(m5out):
        warn("{} is failed in simulation with " \
            "benchmark: {}.".format(
                manager.gem5_opt,
                k
            )
        )
        return

    """
        We conduct post-simulation workloads, including
        1. model with the new DEG formulation (optional),
        2. power & area modeling.
    """
    threads = []
    # power & area
    thread = WorkerThread(
        func=pat_model,
        args=(manager, k,)
    )
    threads.append(thread)

    if manager.configs["misc-setting"]["deg-model"]:
        # trim the trace file
        trim_instruction_flow(m5out)
        # model with the new DEG formulation
        thread = WorkerThread(
            func=deg_model,
            args=(manager.deg_manager, k,)
        )
        threads.append(thread)

    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()


def simulation_spec2006_impl(
    embedding: List[int], manager: object, k: str, v: Dict
) -> NoReturn:
    """
        `k` is the benchmark's name, and `v` is
        meta information of `k`.
    """
    # `m5out` is the target output directory
    m5out = os.path.join(manager.temp, k)

    # change the execution directory
    cmd = "cd {} && {} ".format(
        v["benchmark-root"],
        os.path.join(
            manager.macros["gem5-research-root"],
            "build", "RISCV", manager.gem5_opt
        )
    )

    if manager.configs["misc-setting"]["deg-model"]:
        """
            If we use the new DEG to model, we need
            to generate the trace.
        """
        cmd = "{} --debug-file=instruction-flow  " \
            "--debug-flags=Exec ".format(cmd)
    cmd = "{} " \
        "--outdir={} " \
        "{} " \
        "--num-cpus=1 " \
        "--cpu-type=RiscvO3CPU " \
        "--cmd={} ".format(
            cmd,
            os.path.join(
                manager.macros["gem5-research-root"], k
            ),
            os.path.join(
                manager.macros["gem5-research-root"],
                "configs", "example", "se.py"
            ),
            v["elf"]
        )
    # append benchmark's options/inputs
    if v["options"] is not None:
        cmd = "{} " \
            "--options=\"{}\"".format(
                cmd,
                v["options"]
            )
    cmd = "{} " \
        "--caches " \
        "--cacheline_size=64 " \
        "--l1i_size={}kB " \
        "--l1i_assoc={} " \
        "--l1d_size={}kB " \
        "--l1d_assoc={} " \
        "--bp-type=TournamentBP " \
        "--sys-clock=1GHz " \
        "--cpu-clock=1GHz " \
        "--sys-voltage=0.63V " \
        "--mem-size=16GB " \
        "--mem-type=LPDDR3_1600_1x32 " \
        "--mem-channels=1 " \
        "--warmup-insts={} " \
        "--fast-forward={} " \
        "--maxinsts={}".format(
            cmd,
            embedding[18],
            embedding[19],
            embedding[20],
            embedding[21],
            v["warmup-insts"],
            v["fast-forward"],
            v["maxinsts"]
        )
    cmd = "{} && rm -rf {} && mv -f {} {}".format(
        cmd, m5out, os.path.join(
            manager.macros["gem5-research-root"], k
        ), m5out
    )

    # simulate
    execute(cmd)

    if not if_exist(m5out):
        warn("{} is failed in simulation with " \
            "benchmark: {}.".format(
                manager.gem5_opt,
                k
            )
        )
        return

    """
        We conduct post-simulation workloads, including
        1. model with the new DEG formulation (optional),
        2. power & area modeling.
    """
    threads = []
    # power & area
    thread = WorkerThread(
        func=pat_model,
        args=(manager, k,)
    )
    threads.append(thread)

    if manager.configs["misc-setting"]["deg-model"]:
        # trim the trace file
        trim_instruction_flow(m5out)
        # model with the new DEG formulation
        thread = WorkerThread(
            func=deg_model,
            args=(manager.deg_manager, k,)
        )
        threads.append(thread)

    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()


def simulation_bare_model_impl(
    embedding: List[int], manager: object, k: str, v: Dict
) -> NoReturn:
    """
        `k` is the benchmark's name, and `v` is
        meta information of `k`.
    """
    threads = []
    m5out = os.path.join(
        manager.temp, remove_suffix(k, ".riscv")
    )

    # change the execution directory
    cmd = "cd {} && {} ".format(
        manager.macros["gem5-research-root"],
        os.path.join(
            manager.macros["gem5-research-root"],
            "build", "RISCV", manager.gem5_opt
        )
    )

    if manager.configs["misc-setting"]["deg-model"]:
        """
            If we use the new DEG to model, we need
            to generate the trace.
        """
        cmd = "{} --debug-file=instruction-flow  " \
            "--debug-flags=Exec ".format(cmd)

    cmd = "{} " \
        "--outdir={} " \
        "{} " \
        "--num-cpus=1 " \
        "--cpu-type=RiscvO3CPU " \
        "--cmd={} " \
        "--caches " \
        "--cacheline_size=64 " \
        "--l1i_size={}kB " \
        "--l1i_assoc={} " \
        "--l1d_size={}kB " \
        "--l1d_assoc={} " \
        "--bp-type=TournamentBP " \
        "--sys-clock=1GHz " \
        "--cpu-clock=1GHz " \
        "--sys-voltage=0.63V " \
        "--mem-size=4096MB " \
        "--mem-type=LPDDR3_1600_1x32 " \
        "--mem-channels=1 ".format(
            cmd,
            os.path.join(
                manager.macros["gem5-research-root"],
                remove_suffix(k, ".riscv")
            ),
            os.path.join(
                manager.macros["gem5-research-root"],
                "configs", "example", "se.py"
            ),
            v["elf"],
            embedding[18],
            embedding[19],
            embedding[20],
            embedding[21]
        )
    if manager.benchmark.warmup_insts is not None and \
        manager.benchmark.fast_forward is not None:
        cmd = "{} " \
            "--warmup-insts={} " \
            "--fast-forward={} ".format(
                cmd,
                manager.benchmark.warmup_insts,
                manager.benchmark.fast_forward
            ) 
    cmd = "{} && rm -rf {} && mv -f {} {}".format(
        cmd, m5out, os.path.join(
            manager.macros["gem5-research-root"],
            remove_suffix(k, ".riscv")
        ), m5out
    )

    # simulate
    execute(cmd)

    if not if_exist(m5out):
        warn("{} is failed in simulation with " \
            "benchmark: {}.".format(
                manager.gem5_opt,
                k
            )
        )
        return

    """
        We conduct post-simulation workloads, including
        1. model with the new DEG formulation (optional),
        2. power & area modeling.
    """
    threads = []
    # power & area
    thread = WorkerThread(
        func=pat_model,
        args=(manager,
            remove_suffix(k, ".riscv"),
        )
    )
    threads.append(thread)

    if manager.configs["misc-setting"]["deg-model"]:
        # trim the trace file
        """
            TODO: original implementation
            ```Python
            # trim the trace file
            f = os.path.join(m5out, "instruction-flow")
            with open(f, 'r') as fin:
                cnt = fin.readlines()
                with open(f, 'w') as fout:
                    fout.write(''.join(cnt[o3_simulation.benchmark["warmup-insts"] + 1:]))
            ```
        """
        trim_instruction_flow(m5out)
        # model with the new DEG formulation
        thread = WorkerThread(
            func=deg_model,
            args=(manager.deg_manager,
                remove_suffix(k, ".riscv"),
            )
        )
        threads.append(thread)

    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()


def simulation_spec2017(embedding: List[int], manager: object) -> NoReturn:
    """
        For each benchmark, we launch a thread to execute.
    """
    threads = []
    for k, v in manager.benchmark:
        thread = WorkerThread(
            func=simulation_spec2017_impl,
            args=(embedding, manager, k, v,)
        )
        threads.append(thread)
        thread.start()
    for thread in threads:
        thread.join()


def simulation_spec2006(embedding: List[int], manager: object) -> NoReturn:
    """
        For each benchmark, we launch a thread to execute.
    """
    threads = []
    for k, v in manager.benchmark:
        thread = WorkerThread(
            func=simulation_spec2006_impl,
            args=(embedding, manager, k, v,)
        )
        threads.append(thread)
        thread.start()
    for thread in threads:
        thread.join()


def simulation_bare_model(embedding: List[int], manager: object) -> NoReturn:
    """
        For each benchmark, we launch a thread to execute.
    """
    threads = []
    for k, v in manager.benchmark:
        thread = WorkerThread(
            func=simulation_bare_model_impl,
            args=(embedding, manager, k, v,)
        )
        threads.append(thread)
        thread.start()
    for thread in threads:
        thread.join()


def deg_model(manager: object, benchmark: str) -> NoReturn:
    return manager.model(benchmark)


class EnvManager(object):
    """
        Python environment manager.
    """
    def __init__(self, root):
        super(EnvManager, self).__init__()
        self.root = root

    def generate_pyenv(self) -> str:
        assert self.root.endswith("arch-explorer")
        return "export PYTHONPATH={}".format(
                self.root
            )
        

class PyDEGManager(object):
    """
        A proxy manager designed for DEG.
    """
    def __init__(self, simulator: object):
        super(PyDEGManager, self).__init__()
        self.simulator = simulator
        self.macros = simulator.macros
        self.temp = simulator.temp
        # `env_manager` helps to export `PYTHONPATH`
        self.env_manager = EnvManager(
            self.macros["arch-explorer-root"]
        )

    def model(self, benchmark: str) -> NoReturn:
        output = os.path.join(
            self.temp,
            remove_suffix(benchmark, ".riscv"),
            "analysis.rpt"
        )
        """
            NOTICE: currently, our DEG model is implemented
            with Python. In the future, we will use C++ to
            improve its efficiency.
        """
        cmd = "{}; {} {} -t {} -o {}".format(
            self.env_manager.generate_pyenv(),
            sys.executable,
            os.path.join(self.macros["pydeg-root"], "deg.py"),
            os.path.join(
                self.temp,
                remove_suffix(benchmark, ".riscv"),
                "instruction-flow"
            ),
            output
        )
        if self.simulator.configs["misc-setting"]["vis"]:
            cmd = "{} -v -s {} -e {}".format(
                cmd,
                self.simulator.configs["misc-setting"]["start-idx"],
                self.simulator.configs["misc-setting"]["end-idx"]
            )

        # model with the new DEG formulation
        execute(cmd)

        if not if_exist(output):
            error("DEG is failed with " \
                "benchmark: {}.".format(benchmark)
            )


class CppDEGManager(object):
    """
        A proxy manager designed for DEG.
    """
    def __init__(self, simulator: object):
        super(CppDEGManager, self).__init__()
        self.simulator = simulator
        self.macros = simulator.macros
        self.temp = simulator.temp
        # `env_manager` helps to export `PYTHONPATH`
        self.env_manager = EnvManager(
            self.macros["arch-explorer-root"]
        )

    def model(self, benchmark: str) -> NoReturn:
        output = os.path.join(
            self.temp,
            remove_suffix(benchmark, ".riscv"),
            "analysis.rpt"
        )
        """
            NOTICE: currently, our DEG model is implemented
            with Python. In the future, we will use C++ to
            improve its efficiency.
        """
        cmd = "{} -t {} -o {}".format(
            os.path.join(self.macros["cppdeg-root"], "deg"),
            os.path.join(
                self.temp,
                remove_suffix(benchmark, ".riscv"),
                "instruction-flow"
            ),
            output
        )
        if self.simulator.configs["misc-setting"]["vis"]:
            cmd = "{} -v -s {} -e {}".format(
                cmd,
                self.simulator.configs["misc-setting"]["start-idx"],
                self.simulator.configs["misc-setting"]["end-idx"]
            )

        # model with the new DEG formulation
        execute(cmd)

        if not if_exist(output):
            error("DEG is failed with " \
                "benchmark: {}.".format(benchmark)
            )


class O3CPUSimulation(Simulation):

    class GEM5Manager(object):
        """
            A proxy manager to manage GEM5
        """
        def __init__(self, simulator: object):
            super(O3CPUSimulation.GEM5Manager, self).__init__()
            self.simulator = simulator
            self.configs = self.simulator.configs
            self.macros = self.simulator.macros
            self.benchmark = self.simulator.benchmark
            # `gem5_opt` saves simulator's name
            self.gem5_opt = self.simulator.gem5_opt
            # `temp` saves the temporary root directory
            self.temp = self.simulator.temp
            # `deg_manager` helps to call the new DEG model
            self.deg_manager = PyDEGManager(simulator)
            # for higher performance, we can use `CppDEGManager`
            # self.deg_manager = CppDEGManager(simulator)
            # `env_manager` helps to export `PYTHONPATH`
            self.env_manager = EnvManager(
                self.macros["arch-explorer-root"]
            )

        def modify_gem5_source_code(
            self,
            src: str,
            pattern: Union[str, re.Pattern],
            target: str,
            count: int = 0
        ) -> NoReturn:
            # count = 0 means to replace every `pattern`
            # if `pattern` is matched in `src`
            cnt = open(src, "r+").read()
            with open(src, 'w') as f:
                if isinstance(pattern, str):
                    f.write(re.sub(r"{}".format(pattern), target, cnt, count))
                else:
                    assert isinstance(pattern, re.Pattern), \
                        assert_error("re.Pattern is expected.")
                    f.write(re.sub(pattern, target, cnt, count))

        def generate_pipeline_width(self, width: int) -> NoReturn:
            # fetch width
            self.modify_gem5_source_code(
                self.macros["base-o3-cpu"],
                "fetchWidth\ =\ Param.Unsigned\(\d+,\ \"Fetch\ width\"\)",
                "fetchWidth = Param.Unsigned({}, \"Fetch width\")".format(width)
            )
            # decode width
            self.modify_gem5_source_code(
                self.macros["base-o3-cpu"],
                "decodeWidth\ =\ Param.Unsigned\(\d+,\ \"Decode\ width\"\)",
                "decodeWidth = Param.Unsigned({}, \"Decode width\")".format(width)
            )
            # rename width
            self.modify_gem5_source_code(
                self.macros["base-o3-cpu"],
                "renameWidth\ =\ Param.Unsigned\(\d+,\ \"Rename\ width\"\)",
                "renameWidth = Param.Unsigned({}, \"Rename width\")".format(width)
            )
            # dispatch width
            self.modify_gem5_source_code(
                self.macros["base-o3-cpu"],
                "dispatchWidth\ =\ Param.Unsigned\(\d+,\ \"Dispatch\ width\"\)",
                "dispatchWidth = Param.Unsigned({}, \"Dispatch width\")".format(width)
            )
            # issue width
            self.modify_gem5_source_code(
                self.macros["base-o3-cpu"],
                "issueWidth\ =\ Param.Unsigned\(\d+,\ \"Issue\ width\"\)",
                "issueWidth = Param.Unsigned({}, \"Issue width\")".format(width)
            )
            # writeback width
            self.modify_gem5_source_code(
                self.macros["base-o3-cpu"],
                "wbWidth\ =\ Param.Unsigned\(\d+,\ \"Writeback\ width\"\)",
                "wbWidth = Param.Unsigned({}, \"Writeback width\")".format(width)
            )
            # commit width
            self.modify_gem5_source_code(
                self.macros["base-o3-cpu"],
                "commitWidth\ =\ Param.Unsigned\(\d+,\ \"Commit\ width\"\)",
                "commitWidth = Param.Unsigned({}, \"Commit width\")".format(width)
            )
            # squash width
            self.modify_gem5_source_code(
                self.macros["base-o3-cpu"],
                "squashWidth\ =\ Param.Unsigned\(\d+,\ \"Squash\ width\ width\"\)",
                "squashWidth = Param.Unsigned({}, \"Squash width width\")".format(width)
            )

        def generate_fetch_buffer(self, fetch_buffer_size: int) -> NoReturn:
            # fetch buffer size
            self.modify_gem5_source_code(
                self.macros["base-o3-cpu"],
                "fetchBufferSize\ =\ Param.Unsigned\(\d+,\ \"Fetch\ buffer\ size\ in\ bytes\"\)",
                "fetchBufferSize = Param.Unsigned({}, \"Fetch buffer size in bytes\")".format(fetch_buffer_size)
            )

        def generate_fetch_queue(self, fetch_queue_size: int) -> NoReturn:
            # fetch queue size
            self.modify_gem5_source_code(
                self.macros["base-o3-cpu"],
                "fetchQueueSize\ =\ Param.Unsigned\(\d+,\ \"Fetch\ queue\ size\ in\ micro-ops\ \"",
                "fetchQueueSize = Param.Unsigned({}, \"Fetch queue size in micro-ops \"".format(fetch_queue_size)
            )

        def generate_local_predictor(self, local_predictor_size: int) -> NoReturn:
            # local predictor size
            self.modify_gem5_source_code(
                self.macros["branch-predictor"],
                "localPredictorSize\ =\ Param.Unsigned\(\d+,\ \"Size\ of\ local\ predictor\ for\ TournamentBP\"\)",
                "localPredictorSize = Param.Unsigned({}, \"Size of local predictor for TournamentBP\")".format(local_predictor_size)
            )

        def generate_global_predictor(self, global_predictor_size: int) -> NoReturn:
            # global predictor size
            self.modify_gem5_source_code(
                self.macros["branch-predictor"],
                "globalPredictorSize\ =\ Param.Unsigned\(\d+,\ \"Size\ of\ global\ predictor\ for\ TournamentBP\"\)",
                "globalPredictorSize = Param.Unsigned({}, \"Size of global predictor for TournamentBP\")".format(global_predictor_size)
            )

        def generate_choice_predictor(self, choice_predictor_size: int) -> NoReturn:
            # choice predictor size
            self.modify_gem5_source_code(
                self.macros["branch-predictor"],
                "choicePredictorSize\ =\ Param.Unsigned\(\d+,\ \"Size\ of\ choice\ predictor\ for\ TournamentBP\"\)",
                "choicePredictorSize = Param.Unsigned({}, \"Size of choice predictor for TournamentBP\")".format(choice_predictor_size)
            )

        def generate_ras(self, ras_size: int) -> NoReturn:
            self.modify_gem5_source_code(
                self.macros["branch-predictor"],
                "RASSize\ =\ Param.Unsigned\(\d+,\ \"RAS\ size\"\)",
                "RASSize = Param.Unsigned({}, \"RAS size\")".format(ras_size)
            )

        def generate_btb(self, btb_entry: int) -> NoReturn:
            self.modify_gem5_source_code(
                self.macros["branch-predictor"],
                "BTBEntries\ =\ Param.Unsigned\(\d+,\ \"Number\ of\ BTB\ entries\"\)",
                "BTBEntries = Param.Unsigned({}, \"Number of BTB entries\")".format(btb_entry)
            )

        def generate_rob(self, rob_entry: int) -> NoReturn:
            self.modify_gem5_source_code(
                self.macros["base-o3-cpu"],
                "numROBEntries\ =\ Param.Unsigned\(\d+,\ \"Number\ of\ reorder\ buffer\ entries\"\)",
                "numROBEntries = Param.Unsigned({}, \"Number of reorder buffer entries\")".format(rob_entry)
            )

        def generate_phys_int_reg(self, phys_int_reg: int) -> NoReturn:
            # physical int. register
            self.modify_gem5_source_code(
                self.macros["base-o3-cpu"],
                "numPhysIntRegs\ =\ Param.Unsigned\(\d+,",
                "numPhysIntRegs = Param.Unsigned({},".format(phys_int_reg)
            )

        def generate_phys_fp_reg(self, phys_float_reg: int) -> NoReturn:
            # physical float register
            self.modify_gem5_source_code(
                self.macros["base-o3-cpu"],
                "numPhysFloatRegs\ =\ Param.Unsigned\(\d+,\ \"Number\ of\ physical\ floating\ point\ \"",
                "numPhysFloatRegs = Param.Unsigned({}, \"Number of physical floating point \"".format(phys_float_reg)
            )

        def generate_iq(self, iq_entry: int) -> NoReturn:
            self.modify_gem5_source_code(
                self.macros["base-o3-cpu"],
                "numIQEntries\ =\ Param.Unsigned\(\d+,\ \"Number\ of\ instruction\ queue\ entries\"\)",
                "numIQEntries = Param.Unsigned({}, \"Number of instruction queue entries\")".format(iq_entry)
            )

        def generate_lq(self, lq_entry: int) -> NoReturn:
            # lq entry
            self.modify_gem5_source_code(
                self.macros["base-o3-cpu"],
                "LQEntries\ =\ Param.Unsigned\(\d+,\ \"Number\ of\ load\ queue\ entries\"\)",
                "LQEntries = Param.Unsigned({}, \"Number of load queue entries\")".format(lq_entry)
            )

        def generate_sq(self, sq_entry: int) -> NoReturn:
            # sq entry
            self.modify_gem5_source_code(
                self.macros["base-o3-cpu"],
                "SQEntries\ =\ Param.Unsigned\(\d+,\ \"Number\ of\ store\ queue\ entries\"\)",
                "SQEntries = Param.Unsigned({}, \"Number of store queue entries\")".format(sq_entry)
            )

        def generate_mem_predictor(self, lfst_size: int, ssit_size: int) -> NoReturn:
            # lsft size
            self.modify_gem5_source_code(
                self.macros["base-o3-cpu"],
                "LFSTSize\ =\ Param.Unsigned\(\d+,\ \"Last\ fetched\ store\ table\ size\"\)",
                "LFSTSize = Param.Unsigned({}, \"Last fetched store table size\")".format(lfst_size)
            )
            # ssit size
            self.modify_gem5_source_code(
                self.macros["base-o3-cpu"],
                "SSITSize\ =\ Param.Unsigned\(\d+,\ \"Store\ set\ ID\ table\ size\"\)",
                "SSITSize = Param.Unsigned({}, \"Store set ID table size\")".format(ssit_size)
            )

        def generate_eu(
            self,
            _int_alu: int,
            _int_mult_div: int,
            _fp_alu: int,
            _fp_mult_div: int
        ) -> NoReturn:
            pat = re.compile(r"class DSE_FUPool\(FUPool\):[\w\s\(\[,=\)\]]*")
            int_alu = {3: "IntALU_v1", 4: "IntALU_v2", 5: "IntALU_v3", 6: "IntALU_v4"}
            int_mult_div = {1: "IntMultDiv_v1", 2: "IntMultDiv_v2"}
            fp_alu = {1: "FP_ALU_v1", 2: "FP_ALU_v2", 4: "FP_ALU"}
            fp_mult_div = {1: "FP_MultDiv_v1", 2: "FP_MultDiv_v2"}
            fu_pool = """
class DSE_FUPool(FUPool):
    FUList = [ {}(), {}(), {}(), {}(), RdWrPort_v1() ]
""".format(int_alu[_int_alu], int_mult_div[_int_mult_div], fp_alu[_fp_alu], fp_mult_div[_fp_mult_div])
            self.modify_gem5_source_code(
                self.macros["fu-pool"],
                pat,
                fu_pool
            )

        def generate_mmu(self, size: int) -> NoReturn:
            self.modify_gem5_source_code(
                self.macros["riscv-tlb"],
                "size\ =\ Param.Int\(\d+,\ \"TLB\ size\"\)",
                "size = Param.Int({}, \"TLB size\")".format(size)
            )

        def compile(self) -> NoReturn:
            """
                Notice: the compilation method may
                vary on different platforms.
            """
            cpu_count = multiprocessing.cpu_count()

            hostname = platform.node()
            if "192" in hostname or "B-W389Q6LT-2104.local" in hostname or \
                "MacBook-Pro" in hostname:
                cmd = "cd {} && scons build/RISCV/gem5.opt -j{} ".format(
                    self.macros["gem5-research-root"],
                    cpu_count
                )
            elif "alish-rs" in hostname:
                cmd = "cd {} && export PYTHON_CONFIG=/usr/bin/python3-config; " \
                    "export CCFLAGS_EXTRA=-I/proj/users/chen.bai/tools/protobuf-21.6/build/include; " \
                    "export LINKFLAGS_EXTRA=-L/proj/users/chen.bai/tools/protobuf-21.6/build/lib; " \
                    "scons build/RISCV/gem5.opt -j{} ".format(
                        self.macros["gem5-research-root"],
                        cpu_count
                    )
            else:
                # warn("GEM5 compilation could be failed in {}.".format(hostname))
                # cmd = "cd {} && scons build/RISCV/gem5.opt -j{} ".format(
                #     self.macros["gem5-research-root"],
                #     cpu_count
                # )
                cmd = "cd {} && scons build/RISCV/gem5.opt -j{} ".format(
                    self.macros["gem5-research-root"],
                    cpu_count
                )
            cmd = "{} && mv -f build/RISCV/gem5.opt {}".format(
                cmd,
                os.path.join(
                    self.macros["build-root"],
                    self.gem5_opt
                )
            )
            execute(cmd)

            # check for the compilation
            if not if_exist(os.path.join(
                self.macros["build-root"],
                self.gem5_opt), quiet=False
            ):
                error("{} is failed to generate. Please check your environment"
                    " to make sure you can compile GEM5 successfully!".format(self.gem5_opt)
                )


        def generate_simulator(self, embedding: List[int]) -> NoReturn :
            self.generate_pipeline_width(embedding[0])
            self.generate_fetch_buffer(embedding[1])
            self.generate_fetch_queue(embedding[2])
            self.generate_local_predictor(embedding[3])
            self.generate_global_predictor(embedding[4])
            self.generate_choice_predictor(embedding[5])
            self.generate_ras(embedding[6])
            self.generate_btb(embedding[7])
            self.generate_rob(embedding[8])
            self.generate_phys_int_reg(embedding[9])
            self.generate_phys_fp_reg(embedding[10])
            self.generate_iq(embedding[11])
            self.generate_lq(embedding[12])
            self.generate_sq(embedding[13])
            self.generate_eu(embedding[14], embedding[15], embedding[16], embedding[17])
            self.compile()

        def simulate_spec2006(self):
            pool = ThreadPool(len(self.benchmark.keys()))

        def simulate(self, embedding: List[int]) -> NoReturn:
            if not if_exist(self.temp):
                mkdir(self.temp)
            
            if self.benchmark.name == "spec2017":
                simulation_spec2017(
                    embedding, self
                )
            elif self.benchmark.name == "spec2006":
                simulation_spec2006(
                    embedding, self
                )
            else:
                assert self.benchmark.name == "bare-model"
                simulation_bare_model(
                    embedding, self
                )

    def __init__(
        self,
        design_space: O3CPUDesignSpace,
        configs: Dict
    ):
        super(O3CPUSimulation, self).__init__(configs)
        self.o3cpu_design_space = design_space
        self.benchmark = self.build_benchmark(
            configs["benchmark"]
        )
        # `gem5_opt` saves simulator's name
        self.gem5_opt = None
        # `temp` saves the temporary root directory
        self.temp = None
        # `gem5_manager` saves the instantiation of `GEM5Manager`
        self.gem5_manager = None

    def build_benchmark(self, benchmark: dict) -> Union[List, Dict]:
        # TODO: self.options
        run = benchmark["run"]
        if run == "spec2017":
            return construct_spec2017(benchmark[run])
        elif run == "spec2006":
            return construct_spec2006(benchmark[run])
        else:
            assert run == "bare-model", \
                assert_error("benchmark: {} is unsupported.".format(
                        benchmark
                    )
                )
            return construct_bare_model(benchmark[run])

    def validate_before_simulate(self) -> NoReturn:
        assert self.gem5_opt is not None and \
                self.temp is not None and \
                self.gem5_manager is not None, \
                assert_error("gem5 simulator is not generated.")

    def validate_embedding(self, embedding: List[int]) -> NoReturn:
        """
            Validate if `embedding` is a valid embedding.
        """
        try:
            self.o3cpu_design_space.embedding_to_idx(embedding)
        except ValueError as e:
            error("{} is invalid.".format(embedding))

    def generate_simulator(self, embedding: List[int]) -> NoReturn:
        self.gem5_opt = "gem5-{}.opt".format(
            self.o3cpu_design_space.embedding_to_idx(embedding)
        )
        self.temp = os.path.join(
            self.macros["temp-root"],
            remove_suffix(self.gem5_opt, ".opt")
        )
        self.gem5_manager = self.GEM5Manager(self)
        if if_exist(
            os.path.join(
                self.macros["build-root"],
                self.gem5_opt
            )
        ):
            """
                If the simulator is built, we do not compile
                to generate it again.
            """
            return
        self.gem5_manager.generate_simulator(embedding)

    def simulate_impl(self, embedding: List[int]) -> NoReturn:
        self.validate_before_simulate()
        self.gem5_manager.simulate(embedding)

    def simulate(self, embedding: List[int]):
        self.validate_embedding(embedding)
        self.generate_simulator(embedding)
        self.simulate_impl(embedding)
