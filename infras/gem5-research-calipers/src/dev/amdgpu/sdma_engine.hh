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

#ifndef __DEV_AMDGPU_SDMA_ENGINE_HH__
#define __DEV_AMDGPU_SDMA_ENGINE_HH__

#include "base/bitunion.hh"
#include "debug/SDMAEngine.hh"
#include "dev/amdgpu/amdgpu_device.hh"
#include "dev/amdgpu/sdma_packets.hh"
#include "dev/dma_virt_device.hh"
#include "params/SDMAEngine.hh"

namespace gem5
{

/**
 * System DMA Engine class for AMD dGPU.
 */
class SDMAEngine : public DmaVirtDevice
{
    enum SDMAType
    {
        SDMAGfx,
        SDMAPage
    };

    class SDMAQueue
    {
        Addr _base;
        Addr _rptr;
        Addr _wptr;
        Addr _size;
        bool _valid;
        bool _processing;
        SDMAQueue *_parent;
        SDMAQueue *_ib;
      public:
        SDMAQueue() : _rptr(0), _wptr(0), _valid(false), _processing(false),
            _parent(nullptr), _ib(nullptr) {}

        Addr base() { return _base; }
        Addr rptr() { return _base + _rptr; }
        Addr getRptr() { return _rptr; }
        Addr wptr() { return _base + _wptr; }
        Addr getWptr() { return _wptr; }
        Addr size() { return _size; }
        bool valid() { return _valid; }
        bool processing() { return _processing; }
        SDMAQueue* parent() { return _parent; }
        SDMAQueue* ib() { return _ib; }

        void base(Addr value) { _base = value; }

        void
        incRptr(uint32_t value)
        {
            //assert((_rptr + value) <= (_size << 1));
            _rptr = (_rptr + value) % _size;
        }

        void rptr(Addr value) { _rptr = value; }

        void
        setWptr(Addr value)
        {
            //assert(value <= (_size << 1));
            _wptr = value % _size;
        }

        void wptr(Addr value) { _wptr = value; }

        void size(Addr value) { _size = value; }
        void valid(bool v) { _valid = v; }
        void processing(bool value) { _processing = value; }
        void parent(SDMAQueue* q) { _parent = q; }
        void ib(SDMAQueue* ib) { _ib = ib; }
    };

    /* SDMA Engine ID */
    int id;
    /**
     * Each SDMAEngine processes four queues: paging, gfx, rlc0, and rlc1,
     * where RLC stands for Run List Controller. Each one of these
     * can have one indirect buffer associated at any particular time.
     * The switching order between queues is supposed to be page -> gfx ->
     * rlc0 -> page -> gfx -> rlc1, skipping empty queues.
     */
    SDMAQueue gfx, page, gfxIb, pageIb;
    SDMAQueue rlc0, rlc0Ib, rlc1, rlc1Ib;

    /* Gfx ring buffer registers */
    uint64_t gfxBase;
    uint64_t gfxRptr;
    uint64_t gfxDoorbell;
    uint64_t gfxDoorbellOffset;
    uint64_t gfxWptr;
    /* Page ring buffer registers */
    uint64_t pageBase;
    uint64_t pageRptr;
    uint64_t pageDoorbell;
    uint64_t pageDoorbellOffset;
    uint64_t pageWptr;

    AMDGPUDevice *gpuDevice;
    VegaISA::Walker *walker;

    /* processRLC will select the correct queue for the doorbell */
    std::unordered_map<Addr, int> rlcMap;
    void processRLC0(Addr wptrOffset);
    void processRLC1(Addr wptrOffset);

  public:
    SDMAEngine(const SDMAEngineParams &p);

    void setGPUDevice(AMDGPUDevice *gpu_device);

    void setId(int _id) { id = _id; }
    /**
     * Returns the client id for the Interrupt Handler.
     */
    int getIHClientId();

    /**
     * Methods for translation.
     */
    Addr getGARTAddr(Addr addr) const;
    TranslationGenPtr translate(Addr vaddr, Addr size) override;

    /**
     * Inherited methods.
     */
    Tick write(PacketPtr pkt) override { return 0; }
    Tick read(PacketPtr pkt) override { return 0; }
    AddrRangeList getAddrRanges() const override;
    void serialize(CheckpointOut &cp) const override;
    void unserialize(CheckpointIn &cp) override;

    /**
     * Given a new write ptr offset, communicated to the GPU through a doorbell
     * write, the SDMA engine processes the page, gfx, rlc0, or rlc1 queue.
     */
    void processGfx(Addr wptrOffset);
    void processPage(Addr wptrOffset);
    void processRLC(Addr doorbellOffset, Addr wptrOffset);

    /**
     * This method checks read and write pointers and starts decoding
     * packets if the read pointer is less than the write pointer.
     * It also marks a queue a being currently processing, in case the
     * doorbell is rung again, the newly enqueued packets will be decoded once
     * the currently processing once are finished. This is achieved by calling
     * decodeNext once an entire SDMA packet has been processed.
     */
    void decodeNext(SDMAQueue *q);

    /**
     * Reads the first DW (32 bits) (i.e., header) of an SDMA packet, which
     * encodes the opcode and sub-opcode of the packet. It also creates an
     * SDMA packet object and calls the associated processing function.
     */
    void decodeHeader(SDMAQueue *q, uint32_t data);

    /**
     * Methods that implement processing of SDMA packets
     */
    void write(SDMAQueue *q, sdmaWrite *pkt);
    void writeReadData(SDMAQueue *q, sdmaWrite *pkt, uint32_t *dmaBuffer);
    void writeDone(SDMAQueue *q, sdmaWrite *pkt, uint32_t *dmaBuffer);
    void copy(SDMAQueue *q, sdmaCopy *pkt);
    void copyReadData(SDMAQueue *q, sdmaCopy *pkt, uint8_t *dmaBuffer);
    void copyDone(SDMAQueue *q, sdmaCopy *pkt, uint8_t *dmaBuffer);
    void indirectBuffer(SDMAQueue *q, sdmaIndirectBuffer *pkt);
    void fence(SDMAQueue *q, sdmaFence *pkt);
    void fenceDone(SDMAQueue *q, sdmaFence *pkt);
    void trap(SDMAQueue *q, sdmaTrap *pkt);
    void srbmWrite(SDMAQueue *q, sdmaSRBMWriteHeader *header,
                    sdmaSRBMWrite *pkt);
    void pollRegMem(SDMAQueue *q, sdmaPollRegMemHeader *header,
                    sdmaPollRegMem *pkt);
    void pollRegMemRead(SDMAQueue *q, sdmaPollRegMemHeader *header,
                        sdmaPollRegMem *pkt, uint32_t dma_buffer, int count);
    bool pollRegMemFunc(uint32_t value, uint32_t reference, uint32_t func);
    void ptePde(SDMAQueue *q, sdmaPtePde *pkt);
    void ptePdeDone(SDMAQueue *q, sdmaPtePde *pkt, uint64_t *dmaBuffer);

    /**
     * Methods for getting the values of SDMA MMIO registers.
     */
    uint64_t getGfxBase() { return gfxBase; }
    uint64_t getGfxRptr() { return gfxRptr; }
    uint64_t getGfxDoorbell() { return gfxDoorbell; }
    uint64_t getGfxDoorbellOffset() { return gfxDoorbellOffset; }
    uint64_t getGfxWptr() { return gfxWptr; }
    uint64_t getPageBase() { return pageBase; }
    uint64_t getPageRptr() { return pageRptr; }
    uint64_t getPageDoorbell() { return pageDoorbell; }
    uint64_t getPageDoorbellOffset() { return pageDoorbellOffset; }
    uint64_t getPageWptr() { return pageWptr; }

    /**
     * Methods for setting the values of SDMA MMIO registers.
     */
    void writeMMIO(PacketPtr pkt, Addr mmio_offset);

    void setGfxBaseLo(uint32_t data);
    void setGfxBaseHi(uint32_t data);
    void setGfxRptrLo(uint32_t data);
    void setGfxRptrHi(uint32_t data);
    void setGfxDoorbellLo(uint32_t data);
    void setGfxDoorbellHi(uint32_t data);
    void setGfxDoorbellOffsetLo(uint32_t data);
    void setGfxDoorbellOffsetHi(uint32_t data);
    void setGfxSize(uint64_t data);
    void setGfxWptrLo(uint32_t data);
    void setGfxWptrHi(uint32_t data);
    void setPageBaseLo(uint32_t data);
    void setPageBaseHi(uint32_t data);
    void setPageRptrLo(uint32_t data);
    void setPageRptrHi(uint32_t data);
    void setPageDoorbellLo(uint32_t data);
    void setPageDoorbellHi(uint32_t data);
    void setPageDoorbellOffsetLo(uint32_t data);
    void setPageDoorbellOffsetHi(uint32_t data);
    void setPageSize(uint64_t data);
    void setPageWptrLo(uint32_t data);
    void setPageWptrHi(uint32_t data);

    /**
     * Methods for RLC queues
     */
    void registerRLCQueue(Addr doorbell, Addr rb_base);
    void unregisterRLCQueue(Addr doorbell);

    int cur_vmid = 0;
};

} // namespace gem5

#endif // __DEV_AMDGPU_SDMA_ENGINE_HH__
