/*
 * Copyright (c) 2021 The Regents of the University of California
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "arch/riscv/pmp.hh"

#include "arch/generic/tlb.hh"
#include "arch/riscv/faults.hh"
#include "arch/riscv/isa.hh"
#include "arch/riscv/regs/misc.hh"
#include "base/addr_range.hh"
#include "base/types.hh"
#include "cpu/thread_context.hh"
#include "debug/PMP.hh"
#include "math.h"
#include "mem/request.hh"
#include "params/PMP.hh"
#include "sim/sim_object.hh"

namespace gem5
{

PMP::PMP(const Params &params) :
    SimObject(params),
    pmpEntries(params.pmp_entries),
    numRules(0)
{
    pmpTable.resize(pmpEntries);
}

Fault
PMP::pmpCheck(const RequestPtr &req, BaseMMU::Mode mode,
              RiscvISA::PrivilegeMode pmode, ThreadContext *tc,
              Addr vaddr)
{
    // First determine if pmp table should be consulted
    if (!shouldCheckPMP(pmode, mode, tc))
        return NoFault;

    if (req->hasVaddr()) {
        DPRINTF(PMP, "Checking pmp permissions for va: %#x , pa: %#x\n",
                req->getVaddr(), req->getPaddr());
    }
    else { // this access is corresponding to a page table walk
        DPRINTF(PMP, "Checking pmp permissions for pa: %#x\n",
                req->getPaddr());
    }

    // An access should be successful if there are
    // no rules defined yet or we are in M mode (based
    // on specs v1.10)
    if (numRules == 0 || (pmode == RiscvISA::PrivilegeMode::PRV_M))
        return NoFault;

    // match_index will be used to identify the pmp entry
    // which matched for the given address
    int match_index = -1;

    // all pmp entries need to be looked from the lowest to
    // the highest number
    for (int i = 0; i < pmpTable.size(); i++) {
        AddrRange pmp_range = pmpTable[i].pmpAddr;
        if (pmp_range.contains(req->getPaddr()) &&
                pmp_range.contains(req->getPaddr() + req->getSize())) {
            // according to specs address is only matched,
            // when (addr) and (addr + request_size) are both
            // within the pmp range
            match_index = i;
        }

        if ((match_index > -1)
            && (PMP_OFF != pmpGetAField(pmpTable[match_index].pmpCfg))) {
            // check the RWX permissions from the pmp entry
            uint8_t allowed_privs = PMP_READ | PMP_WRITE | PMP_EXEC;

            // i is the index of pmp table which matched
            allowed_privs &= pmpTable[match_index].pmpCfg;

            if ((mode == BaseMMU::Mode::Read) &&
                                        (PMP_READ & allowed_privs)) {
                return NoFault;
            } else if ((mode == BaseMMU::Mode::Write) &&
                                        (PMP_WRITE & allowed_privs)) {
                return NoFault;
            } else if ((mode == BaseMMU::Mode::Execute) &&
                                        (PMP_EXEC & allowed_privs)) {
                return NoFault;
            } else {
                if (req->hasVaddr()) {
                    return createAddrfault(req->getVaddr(), mode);
                } else {
                    return createAddrfault(vaddr, mode);
                }
            }
        }
    }
    // if no entry matched and we are not in M mode return fault
    if (req->hasVaddr()) {
        return createAddrfault(req->getVaddr(), mode);
    } else {
        return createAddrfault(vaddr, mode);
    }
}

Fault
PMP::createAddrfault(Addr vaddr, BaseMMU::Mode mode)
{
    RiscvISA::ExceptionCode code;
    if (mode == BaseMMU::Read) {
        code = RiscvISA::ExceptionCode::LOAD_ACCESS;
    } else if (mode == BaseMMU::Write) {
        code = RiscvISA::ExceptionCode::STORE_ACCESS;
    } else {
        code = RiscvISA::ExceptionCode::INST_ACCESS;
    }
    warn("pmp access fault.\n");
    return std::make_shared<RiscvISA::AddressFault>(vaddr, code);
}

inline uint8_t
PMP::pmpGetAField(uint8_t cfg)
{
    // to get a field from pmpcfg register
    uint8_t a = cfg >> 3;
    return a & 0x03;
}


void
PMP::pmpUpdateCfg(uint32_t pmp_index, uint8_t this_cfg)
{
    DPRINTF(PMP, "Update pmp config with %u for pmp entry: %u \n",
                                    (unsigned)this_cfg, pmp_index);

    warn_if((PMP_LOCK & this_cfg), "pmp lock feature is not supported.\n");

    pmpTable[pmp_index].pmpCfg = this_cfg;
    pmpUpdateRule(pmp_index);

}

void
PMP::pmpUpdateRule(uint32_t pmp_index)
{
    // In qemu, the rule is updated whenever
    // pmpaddr/pmpcfg is written

    numRules = 0;
    Addr prevAddr = 0;

    if (pmp_index >= 1) {
        prevAddr = pmpTable[pmp_index - 1].rawAddr;
    }

    Addr this_addr = pmpTable[pmp_index].rawAddr;
    uint8_t this_cfg = pmpTable[pmp_index].pmpCfg;
    AddrRange this_range;

    switch (pmpGetAField(this_cfg)) {
      // checking the address matching mode of pmp entry
      case PMP_OFF:
        // null region (pmp disabled)
        this_range = AddrRange(0, 0);
        break;
      case PMP_TOR:
        // top of range mode
        this_range = AddrRange(prevAddr << 2, (this_addr << 2) - 1);
        break;
      case PMP_NA4:
        // naturally aligned four byte region
        this_range = AddrRange(this_addr << 2, (this_addr + 4) - 1);
        break;
      case PMP_NAPOT:
        // naturally aligned power of two region, >= 8 bytes
        this_range = AddrRange(pmpDecodeNapot(this_addr));
        break;
      default:
        this_range = AddrRange(0,0);
    }

    pmpTable[pmp_index].pmpAddr = this_range;

    for (int i = 0; i < pmpEntries; i++) {
        const uint8_t a_field = pmpGetAField(pmpTable[i].pmpCfg);
      if (PMP_OFF != a_field) {
          numRules++;
      }
    }
}

void
PMP::pmpUpdateAddr(uint32_t pmp_index, Addr this_addr)
{
    DPRINTF(PMP, "Update pmp addr %#x for pmp entry %u \n",
                                      this_addr, pmp_index);

    // just writing the raw addr in the pmp table
    // will convert it into a range, once cfg
    // reg is written
    pmpTable[pmp_index].rawAddr = this_addr;
    for (int index = 0; index < pmpEntries; index++) {
        pmpUpdateRule(index);
    }
}

bool
PMP::shouldCheckPMP(RiscvISA::PrivilegeMode pmode,
            BaseMMU::Mode mode, ThreadContext *tc)
{
    // instruction fetch in S and U mode
    bool cond1 = (mode == BaseMMU::Execute &&
            (pmode != RiscvISA::PrivilegeMode::PRV_M));

    // data access in S and U mode when MPRV in mstatus is clear
    RiscvISA::STATUS status =
            tc->readMiscRegNoEffect(RiscvISA::MISCREG_STATUS);
    bool cond2 = (mode != BaseMMU::Execute &&
                 (pmode != RiscvISA::PrivilegeMode::PRV_M)
                 && (!status.mprv));

    // data access in any mode when MPRV bit in mstatus is set
    // and the MPP field in mstatus is S or U
    bool cond3 = (mode != BaseMMU::Execute && (status.mprv)
    && (status.mpp != RiscvISA::PrivilegeMode::PRV_M));

    return (cond1 || cond2 || cond3);
}

AddrRange
PMP::pmpDecodeNapot(Addr pmpaddr)
{
    if (pmpaddr == -1) {
        AddrRange this_range(0, -1);
        return this_range;
    } else {
        uint64_t t1 = ctz64(~pmpaddr);
        uint64_t range = (std::pow(2,t1+3))-1;

        // pmpaddr reg encodes bits 55-2 of a
        // 56 bit physical address for RV64
        uint64_t base = mbits(pmpaddr, 63, t1) << 2;
        AddrRange this_range(base, base+range);
        return this_range;
    }
}

} // namespace gem5
