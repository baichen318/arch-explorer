/*
 * Copyright (c) 2021 Huawei International
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
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

#ifndef __DEV_RISCV_PLIC_HH__
#define __DEV_RISCV_PLIC_HH__

#include <bitset>
#include <map>

#include "arch/riscv/interrupts.hh"
#include "dev/io_device.hh"
#include "dev/reg_bank.hh"
#include "mem/packet.hh"
#include "mem/packet_access.hh"
#include "params/Plic.hh"
#include "sim/system.hh"

namespace gem5
{

using namespace RiscvISA;
/**
 * NOTE:
 * This implementation of CLINT is based on
 * the SiFive U54MC datasheet:
 * https://sifive.cdn.prismic.io/sifive/fab000f6-
 * 0e07-48d0-9602-e437d5367806_sifive_U54MC_rtl_
 * full_20G1.03.00_manual.pdf
 */

/**
 * PLIC Latency Model
 * MMIO changed (aside from threshold)
 * => update internal states
 * => calculate new output
 * => schedule update (3 cycles delay)
 * => update output & schedule next update
 * => update xEIP lines
 *
 * threshold changed
 * => update xEIP lines
 *
 * This ensures cycle-accurate values for
 * MMIO accesses and xEIP lines
 *
 * NOTE:
 * check pending bit when returning maxID
 * to avoid claiming by multiple contexts.
 * Note that pending bits are not propagated
 * through the 3-cycle delay.
 *
 * TODO:
 * - Enforce access control (e.g. avoid S mode
 *   writing to M mode registers)
 */

struct PlicOutput
{
  std::vector<uint32_t> maxID;
  std::vector<uint32_t> maxPriority;
};

class Plic : public BasicPioDevice
{
  // Params
  protected:
    System *system;

    // Number of interrupt sources
    int nSrc;
    /**
     * Number of 32-bit pending registers needed
     * = ceil(nSrc / 32)
     */
    int nSrc32;
    /**
     * Number of interrupt contexts
     * = nThread * 2
     * e.g. context 0 => thread 0 M mode
     *      context 1 => thread 0 S mode
     * This is based on SiFive U54MC datasheet
     */
    int nContext;

  public:
    typedef PlicParams Params;
    Plic(const Params &params);

  // External API
  public:
    /**
     * Interrupt interface
     */
    void post(int src_id);
    void clear(int src_id);

    /**
     * SimObject functions
     */
    void init() override;
    void serialize(CheckpointOut &cp) const override;
    void unserialize(CheckpointIn &cp) override;

  protected:
    /**
     * PioDevice funcitons
     */
    Tick read(PacketPtr pkt) override;
    Tick write(PacketPtr pkt) override;

  // Register bank
  private:

    /**
     * MMIO Registers
     *
     * Priority (0-7):
     * - memory map: 0x0000000 - 0x0000FFC
     *   (1024 sources, 32 bits each)
     * - gem5: vector<uint32_t>
     *   (index: source_id, size: n_src)
     *
     * ... reserved[0]
     *
     * Pending:
     * - memory map: 0x0001000 - 0x0001080
     *   (1024 sources, 1 bit each)
     * - gem5: vector<bitset<32>>
     *   (index: addr_offset, size: ceil(n_src/32))
     *
     * ... reserved[1]
     *
     * Enable:
     * - memory map: 0x0002000 - 0x01F2000
     *   (15872 contexts, 1024 sources, 1 bit each)
     * - gem5: vector<vector<bitset<32>>>
     *   (index: [context_id addr_offset], size: [n_context, ceil(n_src/32)])
     * ... reserved[2]
     *
     * Threshold:
     * - memory map: 0x0200000 - 0x3FFFFFC
     *   (15872 contexts, 32-bit each, 0x1000 byte spacing)
     * - gem5: vector<uint32_t>
     *   (index: context_id, size: n_context)
     *
     * Claim / Complete:
     * - memory map: 0x0200004 - 0x3FFFFFC
     *   (15872 contexts, 32-bit each, 0x1000 byte spacing)
     * - gem5: getter / setter functions
     *
     * ... reserved[3]
     */
    class PlicRegisters: public RegisterBankLE
    {
      public:
        const Addr pendingStart = 0x1000;
        const Addr enableStart = 0x2000;
        const Addr thresholdStart = 0x0200000;
        const Addr enablePadding = 0x80;
        const Addr thresholdPadding = 0x1000;
        const Addr maxBankSize = 0x4000000;


        std::vector<Register32> priority;
        std::vector<Register32> pending;
        std::vector<std::vector<Register32>> enable;
        std::vector<Register32> threshold;
        std::vector<Register32> claim;
        std::vector<RegisterRaz> enable_holes;
        std::vector<RegisterRaz> claim_holes;
        std::vector<RegisterRaz> reserved;

        PlicRegisters(const std::string &name, Addr base, Plic* plic) :
          RegisterBankLE(name, base),
          plic(plic) {}

        Plic* plic;

        void init();

    } registers;

    using Register32 = PlicRegisters::Register32;

    /**
     * Register read / write callbacks
     */
    void writePriority(Register32& reg, const uint32_t& data,
      const int src_id);

    void writeEnable(Register32& reg, const uint32_t& data,
      const int src32_id, const int context_id);

    void writeThreshold(Register32& reg, const uint32_t& data,
      const int context_id);

    uint32_t readClaim(Register32& reg, const int context_id);

    void writeClaim(Register32& reg, const uint32_t& data,
      const int context_id);

  // Latency Model
  private:

    // Internal states
    // per-source pending * priority
    std::vector<uint32_t> pendingPriority;
    // per-context, per-source pendingPriority * enable
    std::vector<std::vector<uint32_t>> effPriority;
    // per-context last-claimed id
    std::vector<uint32_t> lastID;
    PlicOutput output;

    /**
     * Trigger:
     * - Plic::post
     * - Plic::clear
     * - Plic::write
     *
     * Task:
     * - calculate new output
     * - schedule next update event and/or
     *   add new output to outputQueue
     */
    void propagateOutput();
    std::map<Tick, PlicOutput> outputQueue;
    EventFunctionWrapper update;

    /**
     * Trigger:
     * - Plic::update event
     *
     * Task:
     * - set current output to new output
     * - schedule next update event (if any)
     */
    void updateOutput();

    /**
     * Trigger:
     * - Plic::update event
     * - Plic::write
     *
     * Task:
     * - update xEIP lines based on new
     *   output and threshold
     */
    void updateInt();
};

} // namespace gem5

#endif // __DEV_RISCV_PLIC_HH__
