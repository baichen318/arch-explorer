/*
 * Copyright (c) 2021 Advanced Micro Devices, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "dev/amdgpu/sdma_engine.hh"

#include "arch/amdgpu/vega/pagetable_walker.hh"
#include "arch/generic/mmu.hh"
#include "dev/amdgpu/interrupt_handler.hh"
#include "dev/amdgpu/sdma_commands.hh"
#include "dev/amdgpu/sdma_mmio.hh"
#include "mem/packet.hh"
#include "mem/packet_access.hh"
#include "params/SDMAEngine.hh"

namespace gem5
{

SDMAEngine::SDMAEngine(const SDMAEngineParams &p)
    : DmaVirtDevice(p), id(0), gfxBase(0), gfxRptr(0),
      gfxDoorbell(0), gfxDoorbellOffset(0), gfxWptr(0), pageBase(0),
      pageRptr(0), pageDoorbell(0), pageDoorbellOffset(0),
      pageWptr(0), gpuDevice(nullptr), walker(p.walker)
{
    gfx.ib(&gfxIb);
    gfxIb.parent(&gfx);
    gfx.valid(true);
    gfxIb.valid(true);

    page.ib(&pageIb);
    pageIb.parent(&page);
    page.valid(true);
    pageIb.valid(true);

    rlc0.ib(&rlc0Ib);
    rlc0Ib.parent(&rlc0);

    rlc1.ib(&rlc1Ib);
    rlc1Ib.parent(&rlc1);
}

void
SDMAEngine::setGPUDevice(AMDGPUDevice *gpu_device)
{
    gpuDevice = gpu_device;
    walker->setDevRequestor(gpuDevice->vramRequestorId());
}

int
SDMAEngine::getIHClientId()
{
    switch (id) {
      case 0:
        return SOC15_IH_CLIENTID_SDMA0;
      case 1:
        return SOC15_IH_CLIENTID_SDMA1;
      default:
        panic("Unknown SDMA id");
    }
}

Addr
SDMAEngine::getGARTAddr(Addr addr) const
{
    if (!gpuDevice->getVM().inAGP(addr)) {
        Addr low_bits = bits(addr, 11, 0);
        addr = (((addr >> 12) << 3) << 12) | low_bits;
    }
    return addr;
}

/**
 * GPUController will perform DMA operations on VAs, and because
 * page faults are not currently supported for GPUController, we
 * must be able to find the pages mapped for the process.
 */
TranslationGenPtr
SDMAEngine::translate(Addr vaddr, Addr size)
{
    if (gpuDevice->getVM().inAGP(vaddr)) {
        // Use AGP translation gen
        return TranslationGenPtr(
            new AMDGPUVM::AGPTranslationGen(&gpuDevice->getVM(), vaddr, size));
    } else if (gpuDevice->getVM().inMMHUB(vaddr)) {
        // Use MMHUB translation gen
        return TranslationGenPtr(new AMDGPUVM::MMHUBTranslationGen(
                                            &gpuDevice->getVM(), vaddr, size));
    }

    // Assume GART otherwise as this is the only other translation aperture
    // available to the SDMA engine processor.
    return TranslationGenPtr(
        new AMDGPUVM::GARTTranslationGen(&gpuDevice->getVM(), vaddr, size));
}

void
SDMAEngine::registerRLCQueue(Addr doorbell, Addr rb_base)
{
    // Get first free RLC
    if (!rlc0.valid()) {
        DPRINTF(SDMAEngine, "Doorbell %lx mapped to RLC0\n", doorbell);
        rlcMap.insert(std::make_pair(doorbell, 0));
        rlc0.valid(true);
        rlc0.base(rb_base);
        rlc0.rptr(0);
        rlc0.wptr(0);
        rlc0.processing(false);
        // TODO: size - I think pull from MQD 2^rb_cntrl[6:1]-1
        rlc0.size(1024*1024);
    } else if (!rlc1.valid()) {
        DPRINTF(SDMAEngine, "Doorbell %lx mapped to RLC1\n", doorbell);
        rlcMap.insert(std::make_pair(doorbell, 1));
        rlc1.valid(true);
        rlc1.base(rb_base);
        rlc1.rptr(1);
        rlc1.wptr(1);
        rlc1.processing(false);
        // TODO: size - I think pull from MQD 2^rb_cntrl[6:1]-1
        rlc1.size(1024*1024);
    } else {
        panic("No free RLCs. Check they are properly unmapped.");
    }
}

void
SDMAEngine::unregisterRLCQueue(Addr doorbell)
{
    assert(rlcMap.find(doorbell) != rlcMap.end());

    if (rlcMap[doorbell] == 0) {
        rlc0.valid(false);
        rlcMap.erase(doorbell);
    } else if (rlcMap[doorbell] == 1) {
        rlc1.valid(false);
        rlcMap.erase(doorbell);
    } else {
        panic("Cannot unregister unknown RLC queue: %d\n", rlcMap[doorbell]);
    }
}

/* Start decoding packets from the Gfx queue. */
void
SDMAEngine::processGfx(Addr wptrOffset)
{
    gfx.setWptr(wptrOffset);
    if (!gfx.processing()) {
        gfx.processing(true);
        decodeNext(&gfx);
    }
}

/* Start decoding packets from the Page queue. */
void
SDMAEngine::processPage(Addr wptrOffset)
{
    page.setWptr(wptrOffset);
    if (!page.processing()) {
        page.processing(true);
        decodeNext(&page);
    }
}

/* Process RLC queue at given doorbell. */
void
SDMAEngine::processRLC(Addr doorbellOffset, Addr wptrOffset)
{
    assert(rlcMap.find(doorbellOffset) != rlcMap.end());

    if (rlcMap[doorbellOffset] == 0) {
        processRLC0(wptrOffset);
    } else if (rlcMap[doorbellOffset] == 1) {
        processRLC1(wptrOffset);
    } else {
        panic("Cannot process unknown RLC queue: %d\n",
               rlcMap[doorbellOffset]);
    }
}

/* Start decoding packets from the RLC0 queue. */
void
SDMAEngine::processRLC0(Addr wptrOffset)
{
    assert(rlc0.valid());

    rlc0.setWptr(wptrOffset);
    if (!rlc0.processing()) {
        cur_vmid = 1;
        rlc0.processing(true);
        decodeNext(&rlc0);
    }
}

/* Start decoding packets from the RLC1 queue. */
void
SDMAEngine::processRLC1(Addr wptrOffset)
{
    assert(rlc1.valid());

    rlc1.setWptr(wptrOffset);
    if (!rlc1.processing()) {
        cur_vmid = 1;
        rlc1.processing(true);
        decodeNext(&rlc1);
    }
}

/* Decoding next packet in the queue. */
void
SDMAEngine::decodeNext(SDMAQueue *q)
{
    DPRINTF(SDMAEngine, "SDMA decode rptr %p wptr %p\n", q->rptr(), q->wptr());

    if (q->rptr() != q->wptr()) {
        // We are using lambda functions passed to the DmaVirtCallback objects
        // which will call the actuall callback method (e.g., decodeHeader).
        // The dmaBuffer member of the DmaVirtCallback is passed to the lambda
        // function as header in this case.
        auto cb = new DmaVirtCallback<uint32_t>(
            [ = ] (const uint32_t &header)
                { decodeHeader(q, header); });
        dmaReadVirt(q->rptr(), sizeof(uint32_t), cb, &cb->dmaBuffer);
    } else {
        q->processing(false);
        if (q->parent()) {
            DPRINTF(SDMAEngine, "SDMA switching queues\n");
            decodeNext(q->parent());
        }
        cur_vmid = 0;
    }
}

/* Decoding the header of a packet. */
void
SDMAEngine::decodeHeader(SDMAQueue *q, uint32_t header)
{
    q->incRptr(sizeof(header));
    int opcode = bits(header, 7, 0);
    int sub_opcode = bits(header, 15, 8);

    DmaVirtCallback<uint64_t> *cb = nullptr;
    void *dmaBuffer = nullptr;

    DPRINTF(SDMAEngine, "SDMA opcode %p sub-opcode %p\n", opcode, sub_opcode);

    switch(opcode) {
      case SDMA_OP_NOP: {
        uint32_t NOP_count = (header >> 16) & 0x3FFF;
        DPRINTF(SDMAEngine, "SDMA NOP packet with count %d\n", NOP_count);
        if (NOP_count > 0) q->incRptr(NOP_count * 4);
        decodeNext(q);
        } break;
      case SDMA_OP_COPY: {
        DPRINTF(SDMAEngine, "SDMA Copy packet\n");
        switch (sub_opcode) {
          case SDMA_SUBOP_COPY_LINEAR: {
            dmaBuffer = new sdmaCopy();
            cb = new DmaVirtCallback<uint64_t>(
                [ = ] (const uint64_t &)
                    { copy(q, (sdmaCopy *)dmaBuffer); });
            dmaReadVirt(q->rptr(), sizeof(sdmaCopy), cb, dmaBuffer);
            } break;
          case SDMA_SUBOP_COPY_LINEAR_SUB_WIND: {
            panic("SDMA_SUBOP_COPY_LINEAR_SUB_WIND not implemented");
            } break;
          case SDMA_SUBOP_COPY_TILED: {
            panic("SDMA_SUBOP_COPY_TILED not implemented");
            } break;
          case SDMA_SUBOP_COPY_TILED_SUB_WIND: {
            panic("SDMA_SUBOP_COPY_TILED_SUB_WIND not implemented");
            } break;
          case SDMA_SUBOP_COPY_T2T_SUB_WIND: {
            panic("SDMA_SUBOP_COPY_T2T_SUB_WIND not implemented");
            } break;
          case SDMA_SUBOP_COPY_SOA: {
            panic("SDMA_SUBOP_COPY_SOA not implemented");
            } break;
          case SDMA_SUBOP_COPY_DIRTY_PAGE: {
            panic("SDMA_SUBOP_COPY_DIRTY_PAGE not implemented");
            } break;
          case SDMA_SUBOP_COPY_LINEAR_PHY: {
            panic("SDMA_SUBOP_COPY_LINEAR_PHY  not implemented");
            } break;
          default: {
            panic("SDMA unknown copy sub-opcode.");
            } break;
        }
        } break;
      case SDMA_OP_WRITE: {
        DPRINTF(SDMAEngine, "SDMA Write packet\n");
        switch (sub_opcode) {
          case SDMA_SUBOP_WRITE_LINEAR: {
            dmaBuffer = new sdmaWrite();
            cb = new DmaVirtCallback<uint64_t>(
                [ = ] (const uint64_t &)
                    { write(q, (sdmaWrite *)dmaBuffer); });
            dmaReadVirt(q->rptr(), sizeof(sdmaWrite), cb, dmaBuffer);
            } break;
          case SDMA_SUBOP_WRITE_TILED: {
            panic("SDMA_SUBOP_WRITE_TILED not implemented.\n");
            } break;
          default:
            break;
        }
        } break;
      case SDMA_OP_INDIRECT: {
        DPRINTF(SDMAEngine, "SDMA IndirectBuffer packet\n");
        dmaBuffer = new sdmaIndirectBuffer();
        cb = new DmaVirtCallback<uint64_t>(
            [ = ] (const uint64_t &)
                { indirectBuffer(q, (sdmaIndirectBuffer *)dmaBuffer); });
        dmaReadVirt(q->rptr(), sizeof(sdmaIndirectBuffer), cb, dmaBuffer);
        } break;
      case SDMA_OP_FENCE: {
        DPRINTF(SDMAEngine, "SDMA Fence packet\n");
        dmaBuffer = new sdmaFence();
        cb = new DmaVirtCallback<uint64_t>(
            [ = ] (const uint64_t &)
                { fence(q, (sdmaFence *)dmaBuffer); });
        dmaReadVirt(q->rptr(), sizeof(sdmaFence), cb, dmaBuffer);
        } break;
      case SDMA_OP_TRAP: {
        DPRINTF(SDMAEngine, "SDMA Trap packet\n");
        dmaBuffer = new sdmaTrap();
        cb = new DmaVirtCallback<uint64_t>(
            [ = ] (const uint64_t &)
                { trap(q, (sdmaTrap *)dmaBuffer); });
        dmaReadVirt(q->rptr(), sizeof(sdmaTrap), cb, dmaBuffer);
        } break;
      case SDMA_OP_SEM: {
        q->incRptr(sizeof(sdmaSemaphore));
        warn("SDMA_OP_SEM not implemented");
        decodeNext(q);
        } break;
      case SDMA_OP_POLL_REGMEM: {
        DPRINTF(SDMAEngine, "SDMA PollRegMem packet\n");
        sdmaPollRegMemHeader *h = new sdmaPollRegMemHeader();
        *h = *(sdmaPollRegMemHeader *)&header;
        dmaBuffer = new sdmaPollRegMem();
        cb = new DmaVirtCallback<uint64_t>(
            [ = ] (const uint64_t &)
                { pollRegMem(q, h, (sdmaPollRegMem *)dmaBuffer); });
        dmaReadVirt(q->rptr(), sizeof(sdmaPollRegMem), cb, dmaBuffer);
        switch (sub_opcode) {
          case SDMA_SUBOP_POLL_REG_WRITE_MEM: {
            panic("SDMA_SUBOP_POLL_REG_WRITE_MEM not implemented");
            } break;
          case SDMA_SUBOP_POLL_DBIT_WRITE_MEM: {
            panic("SDMA_SUBOP_POLL_DBIT_WRITE_MEM not implemented");
            } break;
          case SDMA_SUBOP_POLL_MEM_VERIFY: {
            panic("SDMA_SUBOP_POLL_MEM_VERIFY not implemented");
            } break;
          default:
            break;
        }
        } break;
      case SDMA_OP_COND_EXE: {
        q->incRptr(sizeof(sdmaCondExec));
        warn("SDMA_OP_SEM not implemented");
        decodeNext(q);
        } break;
      case SDMA_OP_ATOMIC: {
        q->incRptr(sizeof(sdmaAtomic));
        warn("SDMA_OP_ATOMIC not implemented");
        decodeNext(q);
        } break;
      case SDMA_OP_CONST_FILL: {
        q->incRptr(sizeof(sdmaConstFill));
        warn("SDMA_OP_CONST_FILL not implemented");
        decodeNext(q);
        } break;
      case SDMA_OP_PTEPDE: {
        DPRINTF(SDMAEngine, "SDMA PTEPDE packet\n");
        switch (sub_opcode) {
          case SDMA_SUBOP_PTEPDE_GEN:
            DPRINTF(SDMAEngine, "SDMA PTEPDE_GEN sub-opcode\n");
            dmaBuffer = new sdmaPtePde();
            cb = new DmaVirtCallback<uint64_t>(
                [ = ] (const uint64_t &)
                    { ptePde(q, (sdmaPtePde *)dmaBuffer); });
            dmaReadVirt(q->rptr(), sizeof(sdmaPtePde), cb, dmaBuffer);
            break;
          case SDMA_SUBOP_PTEPDE_COPY:
            panic("SDMA_SUBOP_PTEPDE_COPY not implemented");
            break;
          case SDMA_SUBOP_PTEPDE_COPY_BACKWARDS:
            panic("SDMA_SUBOP_PTEPDE_COPY not implemented");
            break;
          case SDMA_SUBOP_PTEPDE_RMW: {
            panic("SDMA_SUBOP_PTEPDE_RMW not implemented");
            } break;
          default:
            DPRINTF(SDMAEngine, "Unsupported PTEPDE sub-opcode %d\n",
                    sub_opcode);
            decodeNext(q);
          break;
        }
        } break;
      case SDMA_OP_TIMESTAMP: {
        q->incRptr(sizeof(sdmaTimestamp));
        switch (sub_opcode) {
          case SDMA_SUBOP_TIMESTAMP_SET: {
            } break;
          case SDMA_SUBOP_TIMESTAMP_GET: {
            } break;
          case SDMA_SUBOP_TIMESTAMP_GET_GLOBAL: {
            } break;
          default:
            break;
        }
        warn("SDMA_OP_TIMESTAMP not implemented");
        decodeNext(q);
        } break;
      case SDMA_OP_SRBM_WRITE: {
        DPRINTF(SDMAEngine, "SDMA SRBMWrite packet\n");
        sdmaSRBMWriteHeader *header = new sdmaSRBMWriteHeader();
        *header = *(sdmaSRBMWriteHeader *)&header;
        dmaBuffer = new sdmaSRBMWrite();
        cb = new DmaVirtCallback<uint64_t>(
            [ = ] (const uint64_t &)
                { srbmWrite(q, header, (sdmaSRBMWrite *)dmaBuffer); });
        dmaReadVirt(q->rptr(), sizeof(sdmaSRBMWrite), cb, dmaBuffer);
        } break;
      case SDMA_OP_PRE_EXE: {
        q->incRptr(sizeof(sdmaPredExec));
        warn("SDMA_OP_PRE_EXE not implemented");
        decodeNext(q);
        } break;
      case SDMA_OP_DUMMY_TRAP: {
        q->incRptr(sizeof(sdmaDummyTrap));
        warn("SDMA_OP_DUMMY_TRAP not implemented");
        decodeNext(q);
        } break;
      default: {
        panic("Invalid SDMA packet.\n");
        } break;
    }
}

/* Implements a write packet. */
void
SDMAEngine::write(SDMAQueue *q, sdmaWrite *pkt)
{
    q->incRptr(sizeof(sdmaWrite));
    // count represents the number of dwords - 1 to write
    pkt->count++;
    DPRINTF(SDMAEngine, "Write %d dwords to %lx\n", pkt->count, pkt->dest);

    // first we have to read needed data from the SDMA queue
    uint32_t *dmaBuffer = new uint32_t[pkt->count];
    auto cb = new DmaVirtCallback<uint64_t>(
        [ = ] (const uint64_t &) { writeReadData(q, pkt, dmaBuffer); });
    dmaReadVirt(q->rptr(), sizeof(uint32_t) * pkt->count, cb,
                (void *)dmaBuffer);
}

/* Completion of data reading for a write packet. */
void
SDMAEngine::writeReadData(SDMAQueue *q, sdmaWrite *pkt, uint32_t *dmaBuffer)
{
    int bufferSize = sizeof(uint32_t) * pkt->count;
    q->incRptr(bufferSize);

    DPRINTF(SDMAEngine, "Write packet data:\n");
    for (int i = 0; i < pkt->count; ++i) {
        DPRINTF(SDMAEngine, "%08x\n", dmaBuffer[i]);
    }

    // lastly we write read data to the destination address
    if (gpuDevice->getVM().inMMHUB(pkt->dest)) {
        Addr mmhubAddr = pkt->dest - gpuDevice->getVM().getMMHUBBase();
        gpuDevice->getMemMgr()->writeRequest(mmhubAddr, (uint8_t *)dmaBuffer,
                                           bufferSize);

        delete []dmaBuffer;
        delete pkt;
        decodeNext(q);
    } else {
        // TODO: getGARTAddr?
        pkt->dest = getGARTAddr(pkt->dest);
        auto cb = new DmaVirtCallback<uint32_t>(
            [ = ] (const uint64_t &) { writeDone(q, pkt, dmaBuffer); });
        dmaWriteVirt(pkt->dest, bufferSize, cb, (void *)dmaBuffer);
    }
}

/* Completion of a write packet. */
void
SDMAEngine::writeDone(SDMAQueue *q, sdmaWrite *pkt, uint32_t *dmaBuffer)
{
    DPRINTF(SDMAEngine, "Write packet completed to %p, %d dwords\n",
            pkt->dest, pkt->count);
    delete []dmaBuffer;
    delete pkt;
    decodeNext(q);
}

/* Implements a copy packet. */
void
SDMAEngine::copy(SDMAQueue *q, sdmaCopy *pkt)
{
    DPRINTF(SDMAEngine, "Copy src: %lx -> dest: %lx count %d\n",
            pkt->source, pkt->dest, pkt->count);
    q->incRptr(sizeof(sdmaCopy));
    // count represents the number of bytes - 1 to be copied
    pkt->count++;
    DPRINTF(SDMAEngine, "Getting GART addr for %lx\n", pkt->source);
    pkt->source = getGARTAddr(pkt->source);
    DPRINTF(SDMAEngine, "GART addr %lx\n", pkt->source);

    // first we have to read needed data from the source address
    uint8_t *dmaBuffer = new uint8_t[pkt->count];
    auto cb = new DmaVirtCallback<uint64_t>(
        [ = ] (const uint64_t &) { copyReadData(q, pkt, dmaBuffer); });
    dmaReadVirt(pkt->source, pkt->count, cb, (void *)dmaBuffer);
}

/* Completion of data reading for a copy packet. */
void
SDMAEngine::copyReadData(SDMAQueue *q, sdmaCopy *pkt, uint8_t *dmaBuffer)
{
    // lastly we write read data to the destination address
    DPRINTF(SDMAEngine, "Copy packet data:\n");
    uint64_t *dmaBuffer64 = new uint64_t[pkt->count/8];
    memcpy(dmaBuffer64, dmaBuffer, pkt->count);
    for (int i = 0; i < pkt->count/8; ++i) {
        DPRINTF(SDMAEngine, "%016lx\n", dmaBuffer64[i]);
    }
    delete [] dmaBuffer64;

    // Aperture is unknown until translating. Do a dummy translation.
    auto tgen = translate(pkt->dest, 64);
    auto addr_range = *(tgen->begin());
    Addr tmp_addr = addr_range.paddr;
    DPRINTF(SDMAEngine, "Tmp addr %#lx -> %#lx\n", pkt->dest, tmp_addr);

    // Writing generated data to the destination address.
    if ((gpuDevice->getVM().inMMHUB(pkt->dest) && cur_vmid == 0) ||
        (gpuDevice->getVM().inMMHUB(tmp_addr) && cur_vmid != 0)) {
        Addr mmhubAddr = 0;
        if (cur_vmid == 0) {
            mmhubAddr = pkt->dest - gpuDevice->getVM().getMMHUBBase();
        } else {
            mmhubAddr = tmp_addr - gpuDevice->getVM().getMMHUBBase();
        }
        DPRINTF(SDMAEngine, "Copying to MMHUB address %#lx\n", mmhubAddr);
        gpuDevice->getMemMgr()->writeRequest(mmhubAddr, dmaBuffer, pkt->count);

        delete pkt;
        decodeNext(q);
    } else {
        auto cb = new DmaVirtCallback<uint64_t>(
            [ = ] (const uint64_t &) { copyDone(q, pkt, dmaBuffer); });
        dmaWriteVirt(pkt->dest, pkt->count, cb, (void *)dmaBuffer);
    }
}

/* Completion of a copy packet. */
void
SDMAEngine::copyDone(SDMAQueue *q, sdmaCopy *pkt, uint8_t *dmaBuffer)
{
    DPRINTF(SDMAEngine, "Copy completed to %p, %d dwords\n",
            pkt->dest, pkt->count);
    delete []dmaBuffer;
    delete pkt;
    decodeNext(q);
}

/* Implements an indirect buffer packet. */
void
SDMAEngine::indirectBuffer(SDMAQueue *q, sdmaIndirectBuffer *pkt)
{
    q->ib()->base(getGARTAddr(pkt->base));
    q->ib()->rptr(0);
    q->ib()->size(pkt->size * sizeof(uint32_t) + 1);
    q->ib()->setWptr(pkt->size * sizeof(uint32_t));

    q->incRptr(sizeof(sdmaIndirectBuffer));

    delete pkt;
    decodeNext(q->ib());
}

/* Implements a fence packet. */
void
SDMAEngine::fence(SDMAQueue *q, sdmaFence *pkt)
{
    q->incRptr(sizeof(sdmaFence));
    pkt->dest = getGARTAddr(pkt->dest);

    // Writing the data from the fence packet to the destination address.
    auto cb = new DmaVirtCallback<uint32_t>(
        [ = ] (const uint32_t &) { fenceDone(q, pkt); }, pkt->data);
    dmaWriteVirt(pkt->dest, sizeof(pkt->data), cb, &cb->dmaBuffer);
}

/* Completion of a fence packet. */
void
SDMAEngine::fenceDone(SDMAQueue *q, sdmaFence *pkt)
{
    DPRINTF(SDMAEngine, "Fence completed to %p, data 0x%x\n",
            pkt->dest, pkt->data);
    delete pkt;
    decodeNext(q);
}

/* Implements a trap packet. */
void
SDMAEngine::trap(SDMAQueue *q, sdmaTrap *pkt)
{
    q->incRptr(sizeof(sdmaTrap));

    DPRINTF(SDMAEngine, "Trap contextId: %p rbRptr: %p ibOffset: %p\n",
            pkt->contextId, pkt->rbRptr, pkt->ibOffset);

    gpuDevice->getIH()->prepareInterruptCookie(pkt->contextId, 0,
            getIHClientId(), TRAP_ID);
    gpuDevice->getIH()->submitInterruptCookie();

    delete pkt;
    decodeNext(q);
}

/* Implements a write SRBM packet. */
void
SDMAEngine::srbmWrite(SDMAQueue *q, sdmaSRBMWriteHeader *header,
                      sdmaSRBMWrite *pkt)
{
    q->incRptr(sizeof(sdmaSRBMWrite));

    [[maybe_unused]] uint32_t reg_addr = pkt->regAddr << 2;
    uint32_t reg_mask = 0x00000000;

    if (header->byteEnable & 0x8) reg_mask |= 0xFF000000;
    if (header->byteEnable & 0x4) reg_mask |= 0x00FF0000;
    if (header->byteEnable & 0x2) reg_mask |= 0x0000FF00;
    if (header->byteEnable & 0x1) reg_mask |= 0x000000FF;
    pkt->data &= reg_mask;

    DPRINTF(SDMAEngine, "SRBM write to %#x with data %#x\n",
            reg_addr, pkt->data);

    warn_once("SRBM write not performed, no SRBM model. This needs to be fixed"
              " if correct system simulation is relying on SRBM registers.");

    delete header;
    delete pkt;
    decodeNext(q);
}

/**
 * Implements a poll reg/mem packet that polls an SRBM register or a memory
 * location, compares the retrieved value with a reference value and if
 * unsuccessfull it retries indefinitely or for a limited number of times.
 */
void
SDMAEngine::pollRegMem(SDMAQueue *q, sdmaPollRegMemHeader *header,
                       sdmaPollRegMem *pkt)
{
    q->incRptr(sizeof(sdmaPollRegMem));

    DPRINTF(SDMAEngine, "POLL_REGMEM: M=%d, func=%d, op=%d, addr=%p, ref=%d, "
            "mask=%p, retry=%d, pinterval=%d\n", header->mode, header->func,
            header->op, pkt->address, pkt->ref, pkt->mask, pkt->retryCount,
            pkt->pollInt);

    bool skip = false;

    if (header->mode == 1) {
        // polling on a memory location
        if (header->op == 0) {
            auto cb = new DmaVirtCallback<uint32_t>(
                [ = ] (const uint32_t &dma_buffer) {
                    pollRegMemRead(q, header, pkt, dma_buffer, 0); });
            dmaReadVirt(pkt->address >> 3, sizeof(uint32_t), cb,
                        (void *)&cb->dmaBuffer);
        } else {
            panic("SDMA poll mem operation not implemented.");
            skip = true;
        }
    } else {
        warn_once("SDMA poll reg is not implemented. If this is required for "
                  "correctness, an SRBM model needs to be implemented.");
        skip = true;
    }

    if (skip) {
        delete header;
        delete pkt;
        decodeNext(q);
    }
}

void
SDMAEngine::pollRegMemRead(SDMAQueue *q, sdmaPollRegMemHeader *header,
                           sdmaPollRegMem *pkt, uint32_t dma_buffer, int count)
{
    assert(header->mode == 1 && header->op == 0);

    if (!pollRegMemFunc(dma_buffer, pkt->ref, header->func) &&
        ((count < (pkt->retryCount + 1) && pkt->retryCount != 0xfff) ||
         pkt->retryCount == 0xfff)) {

        // continue polling on a memory location until reference value is met,
        // retryCount is met or indefinitelly if retryCount is 0xfff
        DPRINTF(SDMAEngine, "SDMA polling mem addr %p, val %d ref %d.\n",
                pkt->address, dma_buffer, pkt->ref);

        auto cb = new DmaVirtCallback<uint32_t>(
            [ = ] (const uint32_t &dma_buffer) {
                pollRegMemRead(q, header, pkt, dma_buffer, count + 1); });
        dmaReadVirt(pkt->address, sizeof(uint32_t), cb,
                    (void *)&cb->dmaBuffer);
    } else {
        DPRINTF(SDMAEngine, "SDMA polling mem addr %p, val %d ref %d done.\n",
                pkt->address, dma_buffer, pkt->ref);

        delete header;
        delete pkt;
        decodeNext(q);
    }
}

bool
SDMAEngine::pollRegMemFunc(uint32_t value, uint32_t reference, uint32_t func)
{
    switch (func) {
      case 0:
        return true;
      break;
      case 1:
        return value < reference;
      break;
      case 2:
        return value <= reference;
      break;
      case 3:
        return value == reference;
      break;
      case 4:
        return value != reference;
      break;
      case 5:
        return value >= reference;
      break;
      case 6:
        return value > reference;
      break;
      default:
        panic("SDMA POLL_REGMEM unknown comparison function.");
      break;
    }
}

/* Implements a PTE PDE generation packet. */
void
SDMAEngine::ptePde(SDMAQueue *q, sdmaPtePde *pkt)
{
    q->incRptr(sizeof(sdmaPtePde));
    pkt->count++;

    DPRINTF(SDMAEngine, "PTEPDE init: %d inc: %d count: %d\n",
            pkt->initValue, pkt->increment, pkt->count);

    // Generating pkt->count double dwords using the initial value, increment
    // and a mask.
    uint64_t *dmaBuffer = new uint64_t[pkt->count];
    for (int i = 0; i < pkt->count; i++) {
        dmaBuffer[i] = (pkt->mask | (pkt->initValue + (i * pkt->increment)));
    }

    // Writing generated data to the destination address.
    if (gpuDevice->getVM().inMMHUB(pkt->dest)) {
        Addr mmhubAddr = pkt->dest - gpuDevice->getVM().getMMHUBBase();
        gpuDevice->getMemMgr()->writeRequest(mmhubAddr, (uint8_t *)dmaBuffer,
                                           sizeof(uint64_t) * pkt->count);

        decodeNext(q);
    } else {
        auto cb = new DmaVirtCallback<uint64_t>(
            [ = ] (const uint64_t &) { ptePdeDone(q, pkt, dmaBuffer); });
        dmaWriteVirt(pkt->dest, sizeof(uint64_t) * pkt->count, cb,
            (void *)dmaBuffer);
    }
}

/* Completion of a PTE PDE generation packet. */
void
SDMAEngine::ptePdeDone(SDMAQueue *q, sdmaPtePde *pkt, uint64_t *dmaBuffer)
{
    DPRINTF(SDMAEngine, "PtePde packet completed to %p, %d 2dwords\n",
            pkt->dest, pkt->count);

    delete []dmaBuffer;
    delete pkt;
    decodeNext(q);
}

AddrRangeList
SDMAEngine::getAddrRanges() const
{
    AddrRangeList ranges;
    return ranges;
}

void
SDMAEngine::serialize(CheckpointOut &cp) const
{
    // Serialize the DmaVirtDevice base class
    DmaVirtDevice::serialize(cp);

    SERIALIZE_SCALAR(gfxBase);
    SERIALIZE_SCALAR(gfxRptr);
    SERIALIZE_SCALAR(gfxDoorbell);
    SERIALIZE_SCALAR(gfxDoorbellOffset);
    SERIALIZE_SCALAR(gfxWptr);
    SERIALIZE_SCALAR(pageBase);
    SERIALIZE_SCALAR(pageRptr);
    SERIALIZE_SCALAR(pageDoorbell);
    SERIALIZE_SCALAR(pageDoorbellOffset);
    SERIALIZE_SCALAR(pageWptr);

    int num_queues = 4;

    std::vector<SDMAQueue *> queues;
    queues.push_back((SDMAQueue *)&gfx);
    queues.push_back((SDMAQueue *)&page);
    queues.push_back((SDMAQueue *)&gfxIb);
    queues.push_back((SDMAQueue *)&pageIb);

    Addr base[num_queues];
    Addr rptr[num_queues];
    Addr wptr[num_queues];
    Addr size[num_queues];
    bool processing[num_queues];

    for (int i = 0; i < num_queues; i++) {
        base[i] = queues[i]->base();
        rptr[i] = queues[i]->getRptr();
        wptr[i] = queues[i]->getWptr();
        size[i] = queues[i]->size();
        processing[i] = queues[i]->processing();
    }

    SERIALIZE_ARRAY(base, num_queues);
    SERIALIZE_ARRAY(rptr, num_queues);
    SERIALIZE_ARRAY(wptr, num_queues);
    SERIALIZE_ARRAY(size, num_queues);
    SERIALIZE_ARRAY(processing, num_queues);
}

void
SDMAEngine::unserialize(CheckpointIn &cp)
{
    // Serialize the DmaVirtDevice base class
    DmaVirtDevice::unserialize(cp);

    UNSERIALIZE_SCALAR(gfxBase);
    UNSERIALIZE_SCALAR(gfxRptr);
    UNSERIALIZE_SCALAR(gfxDoorbell);
    UNSERIALIZE_SCALAR(gfxDoorbellOffset);
    UNSERIALIZE_SCALAR(gfxWptr);
    UNSERIALIZE_SCALAR(pageBase);
    UNSERIALIZE_SCALAR(pageRptr);
    UNSERIALIZE_SCALAR(pageDoorbell);
    UNSERIALIZE_SCALAR(pageDoorbellOffset);
    UNSERIALIZE_SCALAR(pageWptr);

    int num_queues = 4;
    Addr base[num_queues];
    Addr rptr[num_queues];
    Addr wptr[num_queues];
    Addr size[num_queues];
    bool processing[num_queues];

    UNSERIALIZE_ARRAY(base, num_queues);
    UNSERIALIZE_ARRAY(rptr, num_queues);
    UNSERIALIZE_ARRAY(wptr, num_queues);
    UNSERIALIZE_ARRAY(size, num_queues);
    UNSERIALIZE_ARRAY(processing, num_queues);

    std::vector<SDMAQueue *> queues;
    queues.push_back((SDMAQueue *)&gfx);
    queues.push_back((SDMAQueue *)&page);
    queues.push_back((SDMAQueue *)&gfxIb);
    queues.push_back((SDMAQueue *)&pageIb);

    for (int i = 0; i < num_queues; i++) {
        queues[i]->base(base[i]);
        queues[i]->rptr(rptr[i]);
        queues[i]->wptr(wptr[i]);
        queues[i]->size(size[i]);
        queues[i]->processing(processing[i]);
    }
}

void
SDMAEngine::writeMMIO(PacketPtr pkt, Addr mmio_offset)
{
    DPRINTF(SDMAEngine, "Writing offset %#x with data %x\n", mmio_offset,
            pkt->getLE<uint32_t>());

    // In Vega10 headers, the offsets are the same for both SDMAs
    switch (mmio_offset) {
      case mmSDMA_GFX_RB_BASE:
        setGfxBaseLo(pkt->getLE<uint32_t>());
        break;
      case mmSDMA_GFX_RB_BASE_HI:
        setGfxBaseHi(pkt->getLE<uint32_t>());
        break;
      case mmSDMA_GFX_RB_RPTR_ADDR_LO:
        setGfxRptrLo(pkt->getLE<uint32_t>());
        break;
      case mmSDMA_GFX_RB_RPTR_ADDR_HI:
        setGfxRptrHi(pkt->getLE<uint32_t>());
        break;
      case mmSDMA_GFX_DOORBELL:
        setGfxDoorbellLo(pkt->getLE<uint32_t>());
        break;
      case mmSDMA_GFX_DOORBELL_OFFSET:
        setGfxDoorbellOffsetLo(pkt->getLE<uint32_t>());
        // Bit 28 of doorbell indicates that doorbell is enabled.
        if (bits(getGfxDoorbell(), 28, 28)) {
            gpuDevice->setDoorbellType(getGfxDoorbellOffset(),
                                       QueueType::SDMAGfx);
            gpuDevice->setSDMAEngine(getGfxDoorbellOffset(), this);
        }
        break;
      case mmSDMA_GFX_RB_CNTL: {
        uint32_t rb_size = bits(pkt->getLE<uint32_t>(), 6, 1);
        assert(rb_size >= 6 && rb_size <= 62);
        setGfxSize(1 << (rb_size + 2));
      } break;
      case mmSDMA_GFX_RB_WPTR_POLL_ADDR_LO:
        setGfxWptrLo(pkt->getLE<uint32_t>());
        break;
      case mmSDMA_GFX_RB_WPTR_POLL_ADDR_HI:
        setGfxWptrHi(pkt->getLE<uint32_t>());
        break;
      case mmSDMA_PAGE_RB_BASE:
        setPageBaseLo(pkt->getLE<uint32_t>());
        break;
      case mmSDMA_PAGE_RB_RPTR_ADDR_LO:
        setPageRptrLo(pkt->getLE<uint32_t>());
        break;
      case mmSDMA_PAGE_RB_RPTR_ADDR_HI:
        setPageRptrHi(pkt->getLE<uint32_t>());
        break;
      case mmSDMA_PAGE_DOORBELL:
        setPageDoorbellLo(pkt->getLE<uint32_t>());
        break;
      case mmSDMA_PAGE_DOORBELL_OFFSET:
        setPageDoorbellOffsetLo(pkt->getLE<uint32_t>());
        // Bit 28 of doorbell indicates that doorbell is enabled.
        if (bits(getPageDoorbell(), 28, 28)) {
            gpuDevice->setDoorbellType(getPageDoorbellOffset(),
                                       QueueType::SDMAPage);
            gpuDevice->setSDMAEngine(getPageDoorbellOffset(), this);
        }
        break;
      case mmSDMA_PAGE_RB_CNTL: {
        uint32_t rb_size = bits(pkt->getLE<uint32_t>(), 6, 1);
        assert(rb_size >= 6 && rb_size <= 62);
        setPageSize(1 << (rb_size + 2));
      } break;
      case mmSDMA_PAGE_RB_WPTR_POLL_ADDR_LO:
        setPageWptrLo(pkt->getLE<uint32_t>());
        break;
      default:
        DPRINTF(SDMAEngine, "Unknown SDMA MMIO %#x\n", mmio_offset);
        break;
    }
}

void
SDMAEngine::setGfxBaseLo(uint32_t data)
{
    gfxBase = insertBits(gfxBase, 31, 0, 0);
    gfxBase |= data;
    gfx.base((gfxBase >> 1) << 12);
}

void
SDMAEngine::setGfxBaseHi(uint32_t data)
{
    gfxBase = insertBits(gfxBase, 63, 32, 0);
    gfxBase |= ((uint64_t)data) << 32;
    gfx.base((gfxBase >> 1) << 12);
}

void
SDMAEngine::setGfxRptrLo(uint32_t data)
{
    gfxRptr = insertBits(gfxRptr, 31, 0, 0);
    gfxRptr |= data;
}

void
SDMAEngine::setGfxRptrHi(uint32_t data)
{
    gfxRptr = insertBits(gfxRptr, 63, 32, 0);
    gfxRptr |= ((uint64_t)data) << 32;
}

void
SDMAEngine::setGfxDoorbellLo(uint32_t data)
{
    gfxDoorbell = insertBits(gfxDoorbell, 31, 0, 0);
    gfxDoorbell |= data;
}

void
SDMAEngine::setGfxDoorbellHi(uint32_t data)
{
    gfxDoorbell = insertBits(gfxDoorbell, 63, 32, 0);
    gfxDoorbell |= ((uint64_t)data) << 32;
}

void
SDMAEngine::setGfxDoorbellOffsetLo(uint32_t data)
{
    gfxDoorbellOffset = insertBits(gfxDoorbellOffset, 31, 0, 0);
    gfxDoorbellOffset |= data;
}

void
SDMAEngine::setGfxDoorbellOffsetHi(uint32_t data)
{
    gfxDoorbellOffset = insertBits(gfxDoorbellOffset, 63, 32, 0);
    gfxDoorbellOffset |= ((uint64_t)data) << 32;
}

void
SDMAEngine::setGfxSize(uint64_t data)
{
    gfx.size(data);
}

void
SDMAEngine::setGfxWptrLo(uint32_t data)
{
    gfxWptr = insertBits(gfxWptr, 31, 0, 0);
    gfxWptr |= data;
}

void
SDMAEngine::setGfxWptrHi(uint32_t data)
{
    gfxWptr = insertBits(gfxWptr, 31, 0, 0);
    gfxWptr |= ((uint64_t)data) << 32;
}

void
SDMAEngine::setPageBaseLo(uint32_t data)
{
    pageBase = insertBits(pageBase, 31, 0, 0);
    pageBase |= data;
    page.base((pageBase >> 1) << 12);
}

void
SDMAEngine::setPageBaseHi(uint32_t data)
{
    pageBase = insertBits(pageBase, 63, 32, 0);
    pageBase |= ((uint64_t)data) << 32;
    page.base((pageBase >> 1) << 12);
}

void
SDMAEngine::setPageRptrLo(uint32_t data)
{
    pageRptr = insertBits(pageRptr, 31, 0, 0);
    pageRptr |= data;
}

void
SDMAEngine::setPageRptrHi(uint32_t data)
{
    pageRptr = insertBits(pageRptr, 63, 32, 0);
    pageRptr |= ((uint64_t)data) << 32;
}

void
SDMAEngine::setPageDoorbellLo(uint32_t data)
{
    pageDoorbell = insertBits(pageDoorbell, 31, 0, 0);
    pageDoorbell |= data;
}

void
SDMAEngine::setPageDoorbellHi(uint32_t data)
{
    pageDoorbell = insertBits(pageDoorbell, 63, 32, 0);
    pageDoorbell |= ((uint64_t)data) << 32;
}

void
SDMAEngine::setPageDoorbellOffsetLo(uint32_t data)
{
    pageDoorbellOffset = insertBits(pageDoorbellOffset, 31, 0, 0);
    pageDoorbellOffset |= data;
}

void
SDMAEngine::setPageDoorbellOffsetHi(uint32_t data)
{
    pageDoorbellOffset = insertBits(pageDoorbellOffset, 63, 32, 0);
    pageDoorbellOffset |= ((uint64_t)data) << 32;
}

void
SDMAEngine::setPageSize(uint64_t data)
{
    page.size(data);
}

void
SDMAEngine::setPageWptrLo(uint32_t data)
{
    pageWptr = insertBits(pageWptr, 31, 0, 0);
    pageWptr |= data;
}

void
SDMAEngine::setPageWptrHi(uint32_t data)
{
    pageWptr = insertBits(pageWptr, 63, 32, 0);
    pageWptr |= ((uint64_t)data) << 32;
}

} // namespace gem5
