# Copyright (c) 2021 Advanced Micro Devices, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
# contributors may be used to endorse or promote products derived from this
# software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

# System includes
import argparse
import math

# gem5 related
import m5
from m5.objects import *
from m5.util import addToPath

# gem5 options and objects
addToPath('../../')
from ruby import Ruby
from common import Simulation
from common import ObjectList
from common import Options
from common import GPUTLBOptions
from common import GPUTLBConfig
from amd import AmdGPUOptions

# GPU FS related
from system.system import makeGpuFSSystem


def addRunFSOptions(parser):
    parser.add_argument("--script", default=None,
                        help="Script to execute in the simulated system")
    parser.add_argument("--host-parallel", default=False,
                        action="store_true",
                        help="Run multiple host threads in KVM mode")
    parser.add_argument("--restore-dir", type=str, default=None,
                        help="Directory to restore checkpoints from")
    parser.add_argument("--disk-image", default="",
                        help="The boot disk image to mount (/dev/sda)")
    parser.add_argument("--second-disk", default=None,
                        help="The second disk image to mount (/dev/sdb)")
    parser.add_argument("--kernel", default=None, help="Linux kernel to boot")
    parser.add_argument("--gpu-rom", default=None, help="GPU BIOS to load")
    parser.add_argument("--gpu-mmio-trace", default=None,
                        help="GPU MMIO trace to load")
    parser.add_argument("--checkpoint-before-mmios", default=False,
                        action="store_true",
                        help="Take a checkpoint before driver sends MMIOs. "
                        "This is used to switch out of KVM mode and into "
                        "timing mode required to read the VGA ROM on boot.")
    parser.add_argument("--cpu-topology", type=str, default="Crossbar",
                        help="Network topology to use for CPU side. "
                        "Check configs/topologies for complete set")
    parser.add_argument("--gpu-topology", type=str, default="Crossbar",
                        help="Network topology to use for GPU side. "
                        "Check configs/topologies for complete set")
    parser.add_argument("--dgpu-mem-size", action="store", type=str,
                        default="16GB", help="Specify the dGPU physical memory"
                        "  size")
    parser.add_argument("--dgpu-num-dirs", type=int, default=1, help="Set "
                        "the number of dGPU directories (memory controllers")
    parser.add_argument("--dgpu-mem-type", default="HBM_1000_4H_1x128",
                        choices=ObjectList.mem_list.get_names(),
                        help="type of memory to use")

def runGpuFSSystem(args):
    '''
    This function can be called by higher level scripts designed to simulate
    specific devices. As a result the scripts typically hard code some args
    that should not be changed by the user.
    '''

    # These are used by the protocols. They should not be set by the user.
    n_cu = args.num_compute_units
    args.num_sqc = int(math.ceil(float(n_cu) / args.cu_per_sqc))
    args.num_scalar_cache = \
            int(math.ceil(float(n_cu) / args.cu_per_scalar_cache))

    system = makeGpuFSSystem(args)

    root = Root(full_system = True, system = system,
                time_sync_enable = True, time_sync_period = '1000us')

    if args.script is not None:
        system.readfile = args.script

    if args.restore_dir is None:
        m5.instantiate()
    else:
        m5.instantiate(args.restore_dir)


    print("Running the simulation")
    sim_ticks = args.abs_max_tick

    exit_event = m5.simulate(sim_ticks)

    # Keep executing while there is something to do
    while True:
        if exit_event.getCause() == "m5_exit instruction encountered" or \
            exit_event.getCause() == "user interrupt received" or \
            exit_event.getCause() == "simulate() limit reached":
            break
        elif "checkpoint" in exit_event.getCause():
            assert(args.checkpoint_dir is not None)
            m5.checkpoint(args.checkpoint_dir)
            break
        else:
            print('Unknown exit event: %s. Continuing...'
                    % exit_event.getCause())

    print('Exiting @ tick %i because %s' %
          (m5.curTick(), exit_event.getCause()))


if __name__ == "__m5_main__":
    # Add gpufs, common, ruby, amdgpu, and gpu tlb args
    parser = argparse.ArgumentParser()
    addRunFSOptions(parser)
    Options.addCommonOptions(parser)
    Ruby.define_options(parser)
    AmdGPUOptions.addAmdGPUOptions(parser)
    GPUTLBOptions.tlb_options(parser)

    args = parser.parse_args()

    runGpuFSSystem(args)
