/**
 * Copyright (c) Microsoft Corporation.
 * 
 * MIT License
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
/* Author: baichen.bai@alibaba-inc.com */


#ifndef CALIPERS_TYPES_H
#define CALIPERS_TYPES_H

#include "graph_util.h"


enum CacheType
{
    TraceC, // Provided by the trace
    IdealC,
    StatisticalC,
    RealC
};


enum BranchPredictorType
{
    TraceB, // Provided by the trace
    StatisticalB
};


// An ISA might not need all the execution types defined below
enum ExecutionType
{
    IntBase,
    IntMul,
    IntDiv,
    FpBase,
    FpMul,
    FpDiv,
    Load,
    Store,
    BranchCond,
    BranchUncond,
    Syscall,
    Atomic,
    Other
};


// Resource types that may cause structural hazards
enum Resource
{
    RscFetch,
    RscDispatch,
    RscIssue,
    RscMemIssue,
    RscCommit,
    RscMemCommit,
    RscIntAlu,
    RscIntMul,
    RscIntDiv,
    RscIntMulDiv,
    RscFpu,
    RscFpAlu,
    RscFpMul,
    RscFpDiv,
    RscFpMulDiv,
    RscLsu,
};


// Queue types that may cause structural hazards
enum QueueResource
{
    RscInstrQ,
    RscLQ,
    RscSQ,
};


typedef struct INSTRUCTION
{
    uint64_t pc;
    uint32_t bytes;
    uint32_t fetchCycles;
    uint32_t lsCycles;
    bool mispredicted;

    int executionType; // From the ExecutionType enum
    // If an instruction needs multiple execution
    // units, define executionType as an arrays

    uint32_t regReadCount;
    int regRead[MAX_REG_RD];

    uint32_t regWriteCount;
    int regWrite[MAX_REG_WR];

    uint32_t memLoadCount;
    uint64_t memLoadBase;
    uint32_t memLoadLength;

    uint32_t memStoreCount;
    uint64_t memStoreBase;
    uint32_t memStoreLength;

    string inst;
} Instruction;


// A container of different types of instructions
// (Used, e.g., for calculating the breakdown of critical path instructions)
typedef struct INSTRUCTION_TYPES
{
    Vector intInstructions;
    Vector fpInstructions;
    Vector loadInstructions;
    Vector storeInstructions;
    Vector branchInstructions;
    Vector otherInstructions;
} InstructionTypes;


// A container of different types of cycles
// (Used, e.g., for calculating the breakdown of critical path cycles)
typedef struct CYCLE_TYPES
{
    Vector goodFetchHitCycles;
    Vector goodFetchMissCycles;
    Vector badFetchHitCycles;
    Vector badFetchMissCycles;
    Vector decodeCycles;
    Vector dispatchCycles;
    Vector intCycles;
    Vector fpCycles;
    Vector lsCycles;
    Vector loadL1HitCycles;
    Vector loadL2HitCycles;
    Vector loadMissCycles;
    Vector storeL1HitCycles;
    Vector storeL2HitCycles;
    Vector storeMissCycles;
    Vector branchCycles;
    Vector syscallCycles;
    Vector atomicCycles;
    Vector otherCycles;
    Vector commitCycles;
} CycleTypes;



// Different vertex types for different stages of an instruction in the core pipeline
enum VertexType
{
    InstrFetch = 0,
    InstrDispatch = 1,
    InstrExecute = 2,
    MemExecute = 3,
    InstrCommit = 4,
    Last = 4
};

#endif // CALIPERS_TYPES_H
