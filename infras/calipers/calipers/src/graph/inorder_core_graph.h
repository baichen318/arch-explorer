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

#ifndef IN_ORDER_CORE_GRAPH_H
#define IN_ORDER_CORE_GRAPH_H

#include "graph.h"
#include "scoreboard_simple.h"

/**
 * An in-order processor model based on gem5's MinorCPU
 * The current implementation requires that branch prediction
 * result and load/store cycles are provided in the trace.
 */
class InorderCoreGraph : public Graph
{
  private:
    /*** Microarchitectural parameters ***/

    uint32_t fetchBandwidth; // Bandwidth is in instructions per cycle
    uint32_t dispatchBandwidth; // MinorCPU: decodeInputWidth
    uint32_t issueBandwidth; // MinorCPU: executeIssueLimit
    uint32_t commitBandwidth; // MinorCPU: executeCommitLimit
    uint32_t decodeCycles; // MinorCPU: fetch1ToFetch2ForwardDelay + fetch2ToDecodeForwardDelay
    uint32_t dispatchCycles; // MinorCPU: decodeToExecuteForwardDelay
    uint32_t executeToCommitCycles;
    uint32_t predictionCycles;
    uint32_t mispredictionPenalty;
    uint32_t memIssueBandwidth;
    uint32_t memCommitBandwidth;
    uint32_t maxMemAccesses;
    bool loadDependentEarlyIssue;
    bool loadEarlyIssue;
    uint32_t extraLoadLatency;
    ScoreboardSimple scoreboard;  // Also performs bookkeeping


    /*** Bookkeeping ***/

    uint64_t lastMisprediction;
    bool previousInstrMispredicted;
    bool previousWasBranch;
    uint64_t linearPC;

    unordered_map<int, pair<uint64_t, uint32_t>> regLastWrittenBy;
    // Key: Register, Value: <Instruction number, Execution cycles>

    unordered_map<int, bool> regLastWrittenByLoad;
    // Key: Register, Value: Whether it was written by a load

    pair<uint64_t, uint32_t>* ldStWindow; // First: Load/store number, Second: Access cycles
    uint32_t ldStWindowPointer;
    uint64_t lastMemLdSt;
    uint64_t lastLdStCriticalNum;
    uint32_t lastLdStCriticalCycles;

    pair<int, uint32_t> neededRsc[INO_WINDOW];
    // Key: Instruction number % INO_WINDOW
    // Value.first: Type from enum Resource (-1 means don't care)
    // Value.second: The needed resource instance number 


    /*** Graph-related data structures ***/

    IncomingEdge miniGraph[VertexType::Last + 1][MAX_PARENTS];
    // miniGraph[i] = Incoming edges to current instruction's i'th vertex

    uint32_t parents[VertexType::Last + 1];
    // parents[i] = Number of parents of current instruction's i'th vertex


    void initBookKeeping();
    void model(Instruction* instr);
    void modelPipeline(Vertex& fetch_vertex, Vertex& dispatch_vertex,
                       Vertex& execute_vertex, Vertex& mem_vertex,
                       Vertex& commit_vertex, Instruction* instr,
                       uint32_t execution_cycles,
                       unordered_map<uint64_t, uint32_t>& execute_parent);
    void modelMemoryOrderConstraint(Vertex& mem_vertex, bool is_load, bool is_store);
    void trackDataDependencies(Instruction* instr,
                               uint32_t source_independent_cycles,
                               Vertex& execute_vertex,
                               unordered_map<uint64_t, uint32_t>& execute_parent);
    void modelResourceDependenciesSimple(bool is_int, bool is_Int_mul,
                                         bool is_int_div, bool is_fp,
                                         bool is_load_store, Vertex& execute_vertex,
                                         unordered_map<uint64_t, uint32_t>& execute_parent);
    void addEdge(Vertex& parent, OutgoingEdge& e);
    void calculateInstructionCriticalPath();

  public:
    InorderCoreGraph(string trace_file_name,
                     string result_file_name,
                     InstructionStream* instr_stream,
                     uint32_t fetch_bandwidth,
                     uint32_t dispatch_bandwidth,
                     uint32_t issue_bandwidth,
                     uint32_t commit_bandwidth,
                     uint32_t decode_cycles,
                     uint32_t dispatch_cycles,
                     uint32_t execute_to_commit_cycles,
                     uint32_t prediction_cycles,
                     uint32_t misprediction_penalty,
                     uint32_t mem_issue_bandwidth,
                     uint32_t mem_commit_bandwidth,
                     uint32_t max_mem_accesses,
                     uint32_t int_alu_count,
                     uint32_t int_mul_count,
                     uint32_t int_div_count,
                     uint32_t fpu_count,
                     uint32_t lsu_count,
                     bool load_dependent_early_issue,
                     bool load_early_issue);
    ~InorderCoreGraph();
    void run();
};

#endif // IN_ORDER_CORE_GRAPH_H
