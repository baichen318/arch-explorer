# Copyright 2021 Google, Inc.
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

from m5.objects.BaseAtomicSimpleCPU import BaseAtomicSimpleCPU
from m5.objects.BaseNonCachingSimpleCPU import BaseNonCachingSimpleCPU
from m5.objects.BaseTimingSimpleCPU import BaseTimingSimpleCPU
from m5.objects.BaseO3CPU import BaseO3CPU
from m5.objects.BaseMinorCPU import BaseMinorCPU
from m5.objects.RiscvDecoder import RiscvDecoder
from m5.objects.RiscvMMU import RiscvMMU
from m5.objects.RiscvInterrupts import RiscvInterrupts
from m5.objects.RiscvISA import RiscvISA

class RiscvCPU:
    ArchDecoder = RiscvDecoder
    ArchMMU = RiscvMMU
    ArchInterrupts = RiscvInterrupts
    ArchISA = RiscvISA

class RiscvAtomicSimpleCPU(BaseAtomicSimpleCPU, RiscvCPU):
    mmu = RiscvMMU()

class RiscvNonCachingSimpleCPU(BaseNonCachingSimpleCPU, RiscvCPU):
    mmu = RiscvMMU()

class RiscvTimingSimpleCPU(BaseTimingSimpleCPU, RiscvCPU):
    mmu = RiscvMMU()

class RiscvO3CPU(BaseO3CPU, RiscvCPU):
    mmu = RiscvMMU()

class RiscvMinorCPU(BaseMinorCPU, RiscvCPU):
    mmu = RiscvMMU()
