/*
 * Copyright (c) 2012, 2016-2017 ARM Limited
 * Copyright (c) 2013 Advanced Micro Devices, Inc.
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
 * Copyright (c) 2006 The Regents of The University of Michigan
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

#include "cpu/thread_context.hh"

#include <vector>

#include "arch/generic/vec_pred_reg.hh"
#include "base/logging.hh"
#include "base/trace.hh"
#include "config/the_isa.hh"
#include "cpu/base.hh"
#include "debug/Context.hh"
#include "debug/Quiesce.hh"
#include "mem/port.hh"
#include "params/BaseCPU.hh"
#include "sim/full_system.hh"

namespace gem5
{

void
ThreadContext::compare(ThreadContext *one, ThreadContext *two)
{
    const auto &regClasses = one->getIsaPtr()->regClasses();

    DPRINTF(Context, "Comparing thread contexts\n");

    // First loop through the integer registers.
    for (int i = 0; i < regClasses.at(IntRegClass).numRegs(); ++i) {
        RegVal t1 = one->readIntReg(i);
        RegVal t2 = two->readIntReg(i);
        if (t1 != t2)
            panic("Int reg idx %d doesn't match, one: %#x, two: %#x",
                  i, t1, t2);
    }

    // Then loop through the floating point registers.
    for (int i = 0; i < regClasses.at(FloatRegClass).numRegs(); ++i) {
        RegVal t1 = one->readFloatReg(i);
        RegVal t2 = two->readFloatReg(i);
        if (t1 != t2)
            panic("Float reg idx %d doesn't match, one: %#x, two: %#x",
                  i, t1, t2);
    }

    // Then loop through the vector registers.
    const auto &vec_class = regClasses.at(VecRegClass);
    std::vector<uint8_t> vec1(vec_class.regBytes());
    std::vector<uint8_t> vec2(vec_class.regBytes());
    for (int i = 0; i < vec_class.numRegs(); ++i) {
        RegId rid(VecRegClass, i);

        one->getReg(rid, vec1.data());
        two->getReg(rid, vec2.data());
        if (vec1 != vec2) {
            panic("Vec reg idx %d doesn't match, one: %#x, two: %#x",
                  i, vec_class.valString(vec1.data()),
                  vec_class.valString(vec2.data()));
        }
    }

    // Then loop through the predicate registers.
    const auto &vec_pred_class = regClasses.at(VecPredRegClass);
    std::vector<uint8_t> pred1(vec_pred_class.regBytes());
    std::vector<uint8_t> pred2(vec_pred_class.regBytes());
    for (int i = 0; i < vec_pred_class.numRegs(); ++i) {
        RegId rid(VecPredRegClass, i);

        one->getReg(rid, pred1.data());
        two->getReg(rid, pred2.data());
        if (pred1 != pred2) {
            panic("Pred reg idx %d doesn't match, one: %s, two: %s",
                  i, vec_pred_class.valString(pred1.data()),
                  vec_pred_class.valString(pred2.data()));
        }
    }

    for (int i = 0; i < regClasses.at(MiscRegClass).numRegs(); ++i) {
        RegVal t1 = one->readMiscRegNoEffect(i);
        RegVal t2 = two->readMiscRegNoEffect(i);
        if (t1 != t2)
            panic("Misc reg idx %d doesn't match, one: %#x, two: %#x",
                  i, t1, t2);
    }

    // loop through the Condition Code registers.
    for (int i = 0; i < regClasses.at(CCRegClass).numRegs(); ++i) {
        RegVal t1 = one->readCCReg(i);
        RegVal t2 = two->readCCReg(i);
        if (t1 != t2)
            panic("CC reg idx %d doesn't match, one: %#x, two: %#x",
                  i, t1, t2);
    }
    if (one->pcState() != two->pcState())
        panic("PC state doesn't match.");
    int id1 = one->cpuId();
    int id2 = two->cpuId();
    if (id1 != id2)
        panic("CPU ids don't match, one: %d, two: %d", id1, id2);

    const ContextID cid1 = one->contextId();
    const ContextID cid2 = two->contextId();
    if (cid1 != cid2)
        panic("Context ids don't match, one: %d, two: %d", id1, id2);


}

void
ThreadContext::sendFunctional(PacketPtr pkt)
{
    const auto *port =
        dynamic_cast<const RequestPort *>(&getCpuPtr()->getDataPort());
    assert(port);
    port->sendFunctional(pkt);
}

void
ThreadContext::quiesce()
{
    getSystemPtr()->threads.quiesce(contextId());
}


void
ThreadContext::quiesceTick(Tick resume)
{
    getSystemPtr()->threads.quiesceTick(contextId(), resume);
}

RegVal
ThreadContext::getReg(const RegId &reg) const
{
    return getRegFlat(flattenRegId(reg));
}

void *
ThreadContext::getWritableReg(const RegId &reg)
{
    return getWritableRegFlat(flattenRegId(reg));
}

void
ThreadContext::setReg(const RegId &reg, RegVal val)
{
    setRegFlat(flattenRegId(reg), val);
}

void
ThreadContext::getReg(const RegId &reg, void *val) const
{
    getRegFlat(flattenRegId(reg), val);
}

void
ThreadContext::setReg(const RegId &reg, const void *val)
{
    setRegFlat(flattenRegId(reg), val);
}

RegVal
ThreadContext::getRegFlat(const RegId &reg) const
{
    RegVal val;
    getRegFlat(reg, &val);
    return val;
}

void
ThreadContext::setRegFlat(const RegId &reg, RegVal val)
{
    setRegFlat(reg, &val);
}

void
serialize(const ThreadContext &tc, CheckpointOut &cp)
{
    // Cast away the const so we can get the non-const ISA ptr, which we then
    // use to get the const register classes.
    auto &nc_tc = const_cast<ThreadContext &>(tc);
    const auto &regClasses = nc_tc.getIsaPtr()->regClasses();

    const size_t numFloats = regClasses.at(FloatRegClass).numRegs();
    RegVal floatRegs[numFloats];
    for (int i = 0; i < numFloats; ++i)
        floatRegs[i] = tc.readFloatRegFlat(i);
    // This is a bit ugly, but needed to maintain backwards
    // compatibility.
    arrayParamOut(cp, "floatRegs.i", floatRegs, numFloats);

    const size_t numVecs = regClasses.at(VecRegClass).numRegs();
    std::vector<TheISA::VecRegContainer> vecRegs(numVecs);
    for (int i = 0; i < numVecs; ++i) {
        vecRegs[i] = tc.readVecRegFlat(i);
    }
    SERIALIZE_CONTAINER(vecRegs);

    const size_t numPreds = regClasses.at(VecPredRegClass).numRegs();
    std::vector<TheISA::VecPredRegContainer> vecPredRegs(numPreds);
    for (int i = 0; i < numPreds; ++i) {
        tc.getRegFlat(RegId(VecPredRegClass, i), &vecPredRegs[i]);
    }
    SERIALIZE_CONTAINER(vecPredRegs);

    const size_t numInts = regClasses.at(IntRegClass).numRegs();
    RegVal intRegs[numInts];
    for (int i = 0; i < numInts; ++i)
        intRegs[i] = tc.readIntRegFlat(i);
    SERIALIZE_ARRAY(intRegs, numInts);

    const size_t numCcs = regClasses.at(CCRegClass).numRegs();
    if (numCcs) {
        RegVal ccRegs[numCcs];
        for (int i = 0; i < numCcs; ++i)
            ccRegs[i] = tc.readCCRegFlat(i);
        SERIALIZE_ARRAY(ccRegs, numCcs);
    }

    tc.pcState().serialize(cp);

    // thread_num and cpu_id are deterministic from the config
}

void
unserialize(ThreadContext &tc, CheckpointIn &cp)
{
    const auto &regClasses = tc.getIsaPtr()->regClasses();

    const size_t numFloats = regClasses.at(FloatRegClass).numRegs();
    RegVal floatRegs[numFloats];
    // This is a bit ugly, but needed to maintain backwards
    // compatibility.
    arrayParamIn(cp, "floatRegs.i", floatRegs, numFloats);
    for (int i = 0; i < numFloats; ++i)
        tc.setFloatRegFlat(i, floatRegs[i]);

    const size_t numVecs = regClasses.at(VecRegClass).numRegs();
    std::vector<TheISA::VecRegContainer> vecRegs(numVecs);
    UNSERIALIZE_CONTAINER(vecRegs);
    for (int i = 0; i < numVecs; ++i) {
        tc.setVecRegFlat(i, vecRegs[i]);
    }

    const size_t numPreds = regClasses.at(VecPredRegClass).numRegs();
    std::vector<TheISA::VecPredRegContainer> vecPredRegs(numPreds);
    UNSERIALIZE_CONTAINER(vecPredRegs);
    for (int i = 0; i < numPreds; ++i) {
        tc.setRegFlat(RegId(VecPredRegClass, i), &vecPredRegs[i]);
    }

    const size_t numInts = regClasses.at(IntRegClass).numRegs();
    RegVal intRegs[numInts];
    UNSERIALIZE_ARRAY(intRegs, numInts);
    for (int i = 0; i < numInts; ++i)
        tc.setIntRegFlat(i, intRegs[i]);

    const size_t numCcs = regClasses.at(CCRegClass).numRegs();
    if (numCcs) {
        RegVal ccRegs[numCcs];
        UNSERIALIZE_ARRAY(ccRegs, numCcs);
        for (int i = 0; i < numCcs; ++i)
            tc.setCCRegFlat(i, ccRegs[i]);
    }

    std::unique_ptr<PCStateBase> pc_state(tc.pcState().clone());
    pc_state->unserialize(cp);
    tc.pcState(*pc_state);

    // thread_num and cpu_id are deterministic from the config
}

void
takeOverFrom(ThreadContext &ntc, ThreadContext &otc)
{
    assert(ntc.getProcessPtr() == otc.getProcessPtr());

    ntc.setStatus(otc.status());
    ntc.copyArchRegs(&otc);
    ntc.setContextId(otc.contextId());
    ntc.setThreadId(otc.threadId());

    if (FullSystem)
        assert(ntc.getSystemPtr() == otc.getSystemPtr());

    otc.setStatus(ThreadContext::Halted);
}

} // namespace gem5
