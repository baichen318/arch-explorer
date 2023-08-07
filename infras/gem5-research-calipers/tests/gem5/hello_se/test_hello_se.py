# Copyright (c) 2020 The Regents of the University of California
# All Rights Reserved.
#
# Copyright (c) 2020 ARM Limited
# All rights reserved
#
# The license below extends only to copyright in the software and shall
# not be construed as granting a license to any other intellectual
# property including but not limited to intellectual property relating
# to a hardware implementation of the functionality of the software
# licensed hereunder.  You may use the software subject to the license
# terms below provided that you ensure that this notice is replicated
# unmodified and in its entirety in all distributions of the software,
# modified or unmodified, in source code or in binary form.
#
# Copyright (c) 2017 Mark D. Hill and David A. Wood
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""
Test file for the util m5 exit assembly instruction.
"""
from testlib import *

import re

isa_str_map = {
    constants.gcn3_x86_tag: "x86",
    constants.arm_tag: "arm",
    constants.mips_tag: "mips",
    constants.riscv_tag: "riscv",
    constants.sparc_tag: "sparc",
}

static_progs = {
    constants.vega_x86_tag: (
        "x86-hello64-static",
        "x86-hello32-static",
    ),
    constants.arm_tag: (
        "arm-hello64-static",
        "arm-hello32-static",
    ),
    constants.mips_tag: ("mips-hello",),
    constants.riscv_tag: ("riscv-hello",),
    constants.sparc_tag: ("sparc-hello",),
}

dynamic_progs = {constants.vega_x86_tag: ("x86-hello64-dynamic",)}

cpu_types = {
    constants.vega_x86_tag: ("timing", "atomic", "o3"),
    constants.arm_tag: ("timing", "atomic", "o3", "minor"),
    constants.mips_tag: ("timing", "atomic", "o3"),
    constants.riscv_tag: ("timing", "atomic", "o3", "minor"),
    constants.sparc_tag: ("timing", "atomic"),
}

# We only want to test x86, arm, and riscv on quick. Mips and sparc will be
# left for long.
os_length = {
    constants.vega_x86_tag: constants.quick_tag,
    constants.arm_tag: constants.quick_tag,
    constants.mips_tag: constants.long_tag,
    constants.riscv_tag: constants.quick_tag,
    constants.sparc_tag: constants.long_tag,
}


if config.bin_path:
    resource_path = config.bin_path
else:
    resource_path = joinpath(absdirpath(__file__), "..", "resources")


regex = re.compile(r"Hello world!")
stdout_verifier = verifier.MatchRegex(regex)


def verify_config(isa, binary, cpu, hosts):

    gem5_verify_config(
        name="test-" + binary + "-" + cpu,
        fixtures=(),
        verifiers=(stdout_verifier,),
        config=joinpath(
            config.base_dir,
            "tests",
            "gem5",
            "configs",
            "simple_binary_run.py",
        ),
        config_args=[
            binary,
            cpu,
            "--resource-directory",
            resource_path,
            isa_str_map[isa],
        ],
        valid_isas=(isa,),
        valid_hosts=hosts,
        length=os_length[isa],
    )

# Run statically linked hello worlds
for isa in static_progs:
    for binary in static_progs[isa]:
        for cpu in cpu_types[isa]:
            verify_config(isa, binary, cpu, constants.supported_hosts)

# Run dynamically linked hello worlds
for isa in dynamic_progs:
    for binary in dynamic_progs[isa]:
        for cpu in cpu_types[isa]:
            verify_config(isa, binary, cpu, constants.target_host[isa])
