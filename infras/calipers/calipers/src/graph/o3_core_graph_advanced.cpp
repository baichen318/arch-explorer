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

#include <list>

#include "calipers_defs.h"
#include "calipers_types.h"
#include "graph_util.h"
#include "o3_core_graph_advanced.h"
#include "cache.h"
#include "ideal_cache.h"
#include "statistical_cache.h"
#include "real_cache.h"
#include "branch_predictor.h"
#include "statistical_bp.h"

O3CoreGraphAdvanced::O3CoreGraphAdvanced(string trace_file_name,
                                         string result_file_name, 
                                         InstructionStream* instr_stream,
                                         uint32_t instr_buffer_size,
                                         uint32_t instr_queue_size,
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
                                         uint32_t int_alu_count,
                                         uint32_t int_mul_div_count,
                                         uint32_t fp_alu_count,
                                         uint32_t fp_mul_div_count,
                                         uint32_t lsu_count,
                                         uint32_t lq_size,
                                         uint32_t sq_size,
                                         int bp_type,
                                         string bp_config,
                                         int icache_type,
                                         string icache_config,
                                         int dcache_type,
                                         string dcache_config) :
                                         Graph(trace_file_name, result_file_name, instr_stream),
                                         instrBufferSize(instr_buffer_size),
                                         fetchBandwidth(fetch_bandwidth),
                                         dispatchBandwidth(dispatch_bandwidth),
                                         issueBandwidth(issue_bandwidth),
                                         commitBandwidth(commit_bandwidth),
                                         decodeCycles(decode_cycles),
                                         dispatchCycles(dispatch_cycles),
                                         executeToCommitCycles(execute_to_commit_cycles),
                                         predictionCycles(prediction_cycles),
                                         mispredictionPenalty(misprediction_penalty),
                                         memIssueBandwidth(mem_issue_bandwidth),
                                         memCommitBandwidth(mem_commit_bandwidth)
{
    switch (bp_type)
    {
        case BranchPredictorType::TraceB:
            bp = NULL;
            break;
        case BranchPredictorType::StatisticalB:
            bp = new StatisticalBp(bp_config);
            break;
        default:
            CALIPERS_ERROR("Invalid branch prediction model");
    }

    switch (icache_type)
    {
        case CacheType::TraceC:
            icache = NULL;
            break;
        case CacheType::IdealC:
            icache = new IdealCache();
            break;
        case CacheType::StatisticalC:
            icache = new StatisticalCache(icache_config);
            break;
        case CacheType::RealC:
            icache = new RealCache(icache_config);
            break;
        default:
            CALIPERS_ERROR("Invalid I-cache model");
    }

    switch (dcache_type)
    {
        case CacheType::TraceC:
            dcache = NULL;
            break;
        case CacheType::IdealC:
            dcache = new IdealCache();
            break;
        case CacheType::StatisticalC:
            dcache = new StatisticalCache(dcache_config);
            break;
        case CacheType::RealC:
            dcache = new RealCache(dcache_config);
            break;
        default:
            CALIPERS_ERROR("Invalid D-cache model");
    }

    for (uint32_t i = 0; i < VECTOR_WIDTH; ++i)
    {
        // TODO: Parameterize the last two arguments of initResource and setMixedOperation
        // (i.e., latency and pipelined)
        //scoreboard[i].initResource(Resource::RscFetch, fetch_bandwidth, 1, true);
        //scoreboard[i].initResource(Resource::RscDispatch, dispatch_bandwidth, 1, true);
        scoreboard[i].initResource(Resource::RscIssue, issue_bandwidth, 1, true);
        //scoreboard[i].initResource(Resource::RscMemIssue, mem_issue_bandwidth, 1, true);
        //scoreboard[i].initResource(Resource::RscCommit, commit_bandwidth, 1, true);
        //scoreboard[i].initResource(Resource::RscMemCommit, mem_commit_bandwidth, 1, true);
        scoreboard[i].initResource(Resource::RscIntAlu, int_alu_count, 1, true);
        scoreboard[i].initResource(Resource::RscIntMulDiv, int_mul_div_count, 0, false);
        scoreboard[i].setMixedOperation(Resource::RscIntMul, Resource::RscIntMulDiv, 3, true);
        scoreboard[i].setMixedOperation(Resource::RscIntDiv, Resource::RscIntMulDiv, 20, false);
        scoreboard[i].initResource(Resource::RscFpAlu, fp_alu_count, 2, true);
        scoreboard[i].initResource(Resource::RscFpMulDiv, fp_mul_div_count, 0, false);
        scoreboard[i].setMixedOperation(Resource::RscFpMul, Resource::RscFpMulDiv, 4, true);
        scoreboard[i].setMixedOperation(Resource::RscFpDiv, Resource::RscFpMulDiv, 12, false);
        scoreboard[i].initResource(Resource::RscLsu, lsu_count, 1, true);

        scoreboard[i].initQueue(QueueResource::RscInstrQ, instr_queue_size);
        scoreboard[i].initQueue(QueueResource::RscLQ, lq_size);
        scoreboard[i].initQueue(QueueResource::RscSQ, sq_size);
    }

    intAluTotalCycles = scoreboard[0].getResourceLatency(Resource::RscIntAlu);

    ldStWindow = new pair<uint64_t, pair<uint64_t, uint32_t>>[lq_size + sq_size];
    ldStWindowType = new bool[lq_size + sq_size];

    initBookKeeping();
}

O3CoreGraphAdvanced::~O3CoreGraphAdvanced()
{
    delete[] ldStWindow;
    delete[] ldStWindowType;
}

void O3CoreGraphAdvanced::run()
{
    CALIPERS_INFO("Running the graph-based modeler...");

    bool instr_avail = true;
    bool all_scheduled = false;
    uint32_t read_new = 0;
    sys_nanoseconds my_time;

    for (uint32_t i = 0; i < AnalysisWindow; ++i)
    {
        my_time = chrono::system_clock::now();
        Instruction* instr = instrStream->next();
        streamTime += (chrono::system_clock::now() - my_time).count();

        if (instr != NULL)
        {
            model(instr);
            my_time = chrono::system_clock::now();
            calculateInstructionCriticalPath();
            graphAnalysisTime += (chrono::system_clock::now() - my_time).count();
            ++instrCount;
        }
        else
        {
            instr_avail = false;
            break;
        }
    }

    while (instr_avail || !all_scheduled)
    {
        if (instr_avail && (read_new != 0))
        {
            for (uint32_t i = 0; i < read_new; ++i)
            {
                if (instrCount % 100000 == 0)
                {
                    CALIPERS_INFO("*** " << instrCount
                                  << " instructions modeled/analyzed" << endl);
                }

                my_time = chrono::system_clock::now();
                Instruction* instr = instrStream->next();
                streamTime += (chrono::system_clock::now() - my_time).count();

                if (instr != NULL)
                {
                    model(instr);
                    my_time = chrono::system_clock::now();
                    calculateInstructionCriticalPath();
                    graphAnalysisTime += (chrono::system_clock::now() - my_time).count();
                    ++instrCount;
                }
                else
                {
                    instr_avail = false;
                    break;
                }
            }
        }

        pair<uint32_t, bool> state = modelResourceDependencies();
        read_new = state.first;
        all_scheduled = state.second;
    }

    my_time = chrono::system_clock::now();
    recordStats(true, false);
    graphAnalysisTime += (chrono::system_clock::now() - my_time).count();

    CALIPERS_INFO("Instruction stream time: "
                  << (streamTime / 1000000) << " ms" << endl);
    CALIPERS_INFO("Graph construction time: "
                  << (graphConstructionTime / 1000000) << " ms" << endl);
    CALIPERS_INFO("Graph analysis time:     "
                  << (graphAnalysisTime / 1000000) << " ms" << endl);
}

void O3CoreGraphAdvanced::initBookKeeping()
{
    currentIcacheLine = UINT64_MAX;
    lastMisprediction = UINT64_MAX;
    lastBranch = UINT64_MAX;
    previousInstrMispredicted = false;
    previousWasBranch = false;
    lastMemLdSt = UINT64_MAX;
    ldStWindowPointer = 0;
    headInstr = AnalysisWindow;
    headScheduledInstr = 0;
    
    uint32_t ld_st_window_size = scoreboard[0].getQueueSize(QueueResource::RscLQ) +
                                 scoreboard[0].getQueueSize(QueueResource::RscSQ);
    for (uint32_t i = 0; i < ld_st_window_size; ++i)
    {
        ldStWindow[i].first = UINT64_MAX;
    }

    Vertex first_vertex(0, 0);
    Vector zero_vector(0);
    length[first_vertex] = zero_vector;
    criticalPathCycles[first_vertex].goodFetchHitCycles = zero_vector;
    criticalPathCycles[first_vertex].goodFetchMissCycles = zero_vector;
    criticalPathCycles[first_vertex].badFetchHitCycles = zero_vector;
    criticalPathCycles[first_vertex].badFetchMissCycles = zero_vector;
    criticalPathCycles[first_vertex].decodeCycles = zero_vector;
    criticalPathCycles[first_vertex].dispatchCycles = zero_vector;
    criticalPathCycles[first_vertex].intCycles = zero_vector;
    criticalPathCycles[first_vertex].fpCycles = zero_vector;
    criticalPathCycles[first_vertex].lsCycles = zero_vector;
    criticalPathCycles[first_vertex].loadL1HitCycles = zero_vector;
    criticalPathCycles[first_vertex].loadL2HitCycles = zero_vector;
    criticalPathCycles[first_vertex].loadMissCycles = zero_vector;
    criticalPathCycles[first_vertex].storeL1HitCycles = zero_vector;
    criticalPathCycles[first_vertex].storeL2HitCycles = zero_vector;
    criticalPathCycles[first_vertex].storeMissCycles = zero_vector;
    criticalPathCycles[first_vertex].branchCycles = zero_vector;
    criticalPathCycles[first_vertex].syscallCycles = zero_vector;
    criticalPathCycles[first_vertex].atomicCycles = zero_vector;
    criticalPathCycles[first_vertex].otherCycles = zero_vector;
    criticalPathCycles[first_vertex].commitCycles = zero_vector;
    criticalPathInstructions[first_vertex].intInstructions = zero_vector;
    criticalPathInstructions[first_vertex].fpInstructions = zero_vector;
    criticalPathInstructions[first_vertex].loadInstructions = zero_vector;
    criticalPathInstructions[first_vertex].storeInstructions = zero_vector;
    criticalPathInstructions[first_vertex].branchInstructions = zero_vector;
    criticalPathInstructions[first_vertex].otherInstructions = zero_vector;

    for (uint32_t i = 0; i < AnalysisWindow; ++i)
    {
        executionType[i] = -1;
        lsCycles[i] = UINT32_MAX;
        executionCycles[i] = UINT32_MAX;
    }
}

void O3CoreGraphAdvanced::model(Instruction* instr)
{
    sys_nanoseconds my_time = chrono::system_clock::now();

    Vertex fetch_vertex(VertexType::InstrFetch, instrCount);
    Vertex dispatch_vertex(VertexType::InstrDispatch, instrCount);
    Vertex execute_vertex(VertexType::InstrExecute, instrCount);
    Vertex mem_vertex(VertexType::MemExecute, instrCount);
    Vertex commit_vertex(VertexType::InstrCommit, instrCount);

    uint32_t execution_cycles;
    bool store_to_load_forwarding;
    uint32_t ls_cycles;

    bool is_load = (instr->memLoadCount == 1); // Also covers atomic instructions
    bool is_store = (instr->memStoreCount == 1); // Also covers atomic instructions
    bool is_load_store = is_load || is_store;
    bool is_branch = (instr->executionType == ExecutionType::BranchCond) || 
                     (instr->executionType == ExecutionType::BranchUncond);
    bool is_int = (instr->executionType == ExecutionType::IntBase) || is_branch;
    bool is_int_mul = (instr->executionType == ExecutionType::IntMul);
    bool is_int_div = (instr->executionType == ExecutionType::IntDiv);
    bool is_fp = (instr->executionType == ExecutionType::FpBase);
    bool is_fp_mul = (instr->executionType == ExecutionType::FpMul);
    bool is_fp_div = (instr->executionType == ExecutionType::FpDiv);

    executionType[instrCount % AnalysisWindow] = instr->executionType;    

    // 0: int, 1: fp, 2: load, 3: store, 4: branch, 5: other
    if (is_branch)
    {
        ++instructionMix[4];
    }
    else if (is_int || is_int_mul || is_int_div)
    {
        ++instructionMix[0];
    }
    else if (is_fp || is_fp_mul || is_fp_div)
    {
        ++instructionMix[1];
    }
    else if (is_load)
    {
        ++instructionMix[2];
    }
    else if (is_store)
    {
        ++instructionMix[3];
    }
    else
    {
        ++instructionMix[5];
    }

    if (is_load_store)
    {
        if (dcache == NULL)
        {
            store_to_load_forwarding = false;
        }
        else
        {
            store_to_load_forwarding = modelMemoryOrderConstraint(instr, mem_vertex);
        }
    }

    if (is_load)
    {
        if (dcache == NULL)
        {
            ls_cycles = instr->lsCycles;
        }
        else
        {
            ls_cycles = store_to_load_forwarding ?
                0 : dcache->loadCycles(instr->memLoadBase, instr->memLoadLength);
        }
        execution_cycles = scoreboard[0].getResourceLatency(Resource::RscLsu) + ls_cycles;
        lsCycles[instrCount % AnalysisWindow] = ls_cycles;
    }
    else if (is_store)
    {
        if (dcache == NULL)
        {
            ls_cycles = instr->lsCycles;
        }
        else
        {
            ls_cycles = dcache->storeCycles(instr->memStoreBase, instr->memStoreLength);
        }
        // Stores quickly complete
        execution_cycles = scoreboard[0].getResourceLatency(Resource::RscLsu);
        lsCycles[instrCount % AnalysisWindow] = ls_cycles;
    }
    else if (is_int)
    {
        execution_cycles = scoreboard[0].getResourceLatency(Resource::RscIntAlu);
    }
    else if (is_int_mul)
    {
        execution_cycles = scoreboard[0].getResourceLatency(Resource::RscIntMul);
    }
    else if (is_int_div)
    {
        execution_cycles = scoreboard[0].getResourceLatency(Resource::RscIntDiv);
    }
    else if (is_fp)
    {
        execution_cycles = scoreboard[0].getResourceLatency(Resource::RscFpAlu);
    }
    else if (is_fp_mul)
    {
        execution_cycles = scoreboard[0].getResourceLatency(Resource::RscFpMul);
    }
    else if (is_fp_div)
    {
        execution_cycles = scoreboard[0].getResourceLatency(Resource::RscFpDiv);
    }
    else
    {
        execution_cycles = 1;
    }

    /*
    if (is_load_store)
    {
        executionCycles[instrCount % AnalysisWindow] =
            scoreboard[0].getResourceLatency(Resource::RscLsu);
    }
    else
    {
        executionCycles[instrCount % AnalysisWindow] = execution_cycles;
    }
    */
    executionCycles[instrCount % AnalysisWindow] = execution_cycles;

    modelPipeline(fetch_vertex, dispatch_vertex, execute_vertex,
                  mem_vertex, commit_vertex, instr, execution_cycles);

    trackDataDependencies(instr, execute_vertex, mem_vertex);


    // Update bookkeeping variables

    if (is_load_store)
    {
        lastMemLdSt = instrCount;
        ldStWindow[ldStWindowPointer].first = instrCount;
        if (is_load)
        {
            ldStWindow[ldStWindowPointer].second.first = instr->memLoadBase;
            ldStWindow[ldStWindowPointer].second.second = instr->memLoadLength;
            ldStWindowType[ldStWindowPointer] = true;
        }
        else
        {
            ldStWindow[ldStWindowPointer].second.first = instr->memStoreBase;
            ldStWindow[ldStWindowPointer].second.second = instr->memStoreLength;
            ldStWindowType[ldStWindowPointer] = false;
        }
        ldStWindowPointer = (ldStWindowPointer + 1) %
                            (scoreboard[0].getQueueSize(QueueResource::RscLQ) +
                             scoreboard[0].getQueueSize(QueueResource::RscSQ));

        if (ls_cycles > l2dThreshold)
        {
            ++l2dMisses;
        }
        else if (ls_cycles > l1dThreshold)
        {
            ++l1dMisses;
        }
    }

    previousWasBranch = is_branch;
    linearPC = instr->pc + instr->bytes;
    if (is_branch)
    {
        lastBranch = instrCount;
    }

    for (uint32_t i = 0; i < instr->regWriteCount; ++i)
    {
        int reg_write = instr->regWrite[i];
        regLastWrittenBy[reg_write].first = instrCount;
        if (is_load)
        {
            regLastWrittenBy[reg_write].second = ls_cycles;
            regLastWrittenByLoad[reg_write] = true;
        }
        else
        {
            regLastWrittenBy[reg_write].second = execution_cycles;
            regLastWrittenByLoad[reg_write] = false;
        }
    }

    graphConstructionTime += (chrono::system_clock::now() - my_time).count();
}

void O3CoreGraphAdvanced::modelPipeline(Vertex& fetch_vertex, Vertex& dispatch_vertex,
                                        Vertex& execute_vertex, Vertex& mem_vertex,
                                        Vertex& commit_vertex, Instruction* instr,
                                        uint32_t execution_cycles)
{
    bool mispredicted;
    uint32_t fetch_cycles;
    bool is_load_store = (instr->memLoadCount == 1) || 
                         (instr->memStoreCount == 1);
    bool is_branch = (instr->executionType == BranchCond) ||
                     (instr->executionType == BranchUncond);
    bool no_need_for_ino_dispatch = (instrCount == 0) ||
                                    (dispatchBandwidth == 1);
    bool no_need_for_ino_commit = (instrCount == 0) || 
                                  (commitBandwidth == 1);

    // Branch prediction
    mispredicted = previousInstrMispredicted;
    if (bp == NULL)
    {
        previousInstrMispredicted = instr->mispredicted;
    }
    else
    {
        previousInstrMispredicted = is_branch ? bp->mispredicted(instr->pc) : false;
    }

    if (is_branch)
    {
        ++branchCount;
        if (previousInstrMispredicted)
        {
            ++bpMisses;
        }
    }

    // Fetch
    if (icache == NULL)
    {
        fetch_cycles = instr->fetchCycles;
    }
    else
    {
        if (currentIcacheLine != (instr->pc & (UINT64_MAX << CACHE_ADDRESS_ZEROS)))
        {
            fetch_cycles = icache->loadCycles(instr->pc, CACHE_LINE_BYTES);
        }
        else
        {
            fetch_cycles = 0;
        }
    }

    currentIcacheLine = (instr->pc & (UINT64_MAX << CACHE_ADDRESS_ZEROS));

    if (fetch_cycles > l2iThreshold)
    {
        ++l2iMisses;
    }
    else if (fetch_cycles > l1iThreshold)
    {
        ++l1iMisses;
    }

    // Dispatch after fetch
    OutgoingEdge fetch_after_dispatch(dispatch_vertex, (int64_t)decodeCycles);
    //cout << "Dispatch after fetch" << endl;
    addEdge(fetch_vertex, fetch_after_dispatch);

    // Execute after dispatch
    OutgoingEdge execute_after_dispatch(execute_vertex, (int64_t)dispatchCycles);
    //cout << "Execute after dispatch" << endl;
    addEdge(dispatch_vertex, execute_after_dispatch);

    if (is_load_store)
    {
        // Memory execute (actual memory operation) after instruction execute (address calculation)
        OutgoingEdge mem_after_instr(mem_vertex,
            (int64_t)scoreboard[0].getResourceLatency(Resource::RscLsu));
        //cout << "Memory execute after instruction execute" << endl;
        addEdge(execute_vertex, mem_after_instr);

        // Commit after execute
        OutgoingEdge commit_after_execute(commit_vertex,
            (int64_t)(execution_cycles - scoreboard[0].getResourceLatency(Resource::RscLsu) +
                      executeToCommitCycles));
        //cout << "Commit after memory execute" << endl;
        addEdge(mem_vertex, commit_after_execute);        
    }
    else
    {
        // Commit after execute
        OutgoingEdge commit_after_execute(commit_vertex,
            (int64_t)(execution_cycles + executeToCommitCycles));
        //cout << "Commit after execute" << endl;
        addEdge(execute_vertex, commit_after_execute);
    }

    // Limited fetch bandwidth
    if ((instrCount >= fetchBandwidth) &&
        ((lastMisprediction == UINT64_MAX) ||
         (instrCount - lastMisprediction > fetchBandwidth)))
    {
        Vertex prev_fetch_vertex(VertexType::InstrFetch, instrCount - fetchBandwidth);
        OutgoingEdge limited_fetch_bw(fetch_vertex, 1);
        //cout << "Limited fetch bandwidth" << endl;
        addEdge(prev_fetch_vertex, limited_fetch_bw);
    }

    // Limited dispatch bandwidth
    if ((instrCount >= dispatchBandwidth) &&
        ((lastMisprediction == UINT64_MAX) ||
         (instrCount - lastMisprediction > dispatchBandwidth)))
    {
        Vertex prev_dispatch_vertex(VertexType::InstrDispatch, instrCount - dispatchBandwidth);
        OutgoingEdge limited_dispatch_bw(dispatch_vertex, 1);
        //cout << "Limited dispatch bandwidth" << endl;
        addEdge(prev_dispatch_vertex, limited_dispatch_bw);
    }

    // Limited commit bandwidth
    if ((instrCount >= commitBandwidth) &&
        ((lastMisprediction == UINT64_MAX) ||
         (instrCount - lastMisprediction > commitBandwidth)))
    {
        Vertex prev_commit_vertex(VertexType::InstrCommit, instrCount - commitBandwidth);
        OutgoingEdge limited_commit_bw(commit_vertex, 1);
        //cout << "Limited commit bandwidth" << endl;
        addEdge(prev_commit_vertex, limited_commit_bw);
    }

    // Limited memory commit bandwidth
    if ((lastMemLdSt != UINT64_MAX) &&
        (instrCount - lastMemLdSt == memCommitBandwidth) &&
        ((lastMisprediction == UINT64_MAX) ||
         (instrCount - lastMisprediction > memCommitBandwidth)))
    {
        Vertex prev_commit_vertex(VertexType::InstrCommit, lastMemLdSt);
        OutgoingEdge limited_mem_commit_bw(commit_vertex, 1);
        //cout << "Limited memory commit bandwidth" << endl;
        addEdge(prev_commit_vertex, limited_mem_commit_bw);

        no_need_for_ino_commit = no_need_for_ino_commit || ((instrCount - lastMemLdSt) == 1);
    }

    if (mispredicted)
    {
        Vertex prev_branch_vertex(VertexType::InstrExecute, instrCount - 1);
        OutgoingEdge mispredicted_fetch(fetch_vertex,
            (int64_t)(scoreboard[0].getResourceLatency(Resource::RscIntAlu) +
                      mispredictionPenalty + fetch_cycles));
        //cout << "Bad fetch" << endl;
        addEdge(prev_branch_vertex, mispredicted_fetch);
        lastMisprediction = instrCount - 1;
    }
    else
    {
        if (instrCount != 0)
        {
            // In-order fetch
            uint32_t fetch_weight;
            //if (previousWasBranch && (instr->pc != linearPC)) // Correctly taken branch
            if (previousWasBranch)
            {
                fetch_weight = predictionCycles + fetch_cycles;
            }
            else // No branch or correctly not taken branch
            {
                fetch_weight = fetch_cycles;
            }

            Vertex prev_fetch_vertex(VertexType::InstrFetch, instrCount - 1);
            OutgoingEdge in_order_fetch(fetch_vertex, (int64_t)fetch_weight);
            //cout << "Good fetch" << endl;
            addEdge(prev_fetch_vertex, in_order_fetch);
        }

        // In-order dispatch
        if (!no_need_for_ino_dispatch)
        {
            Vertex prev_dispatch_vertex(VertexType::InstrDispatch, instrCount - 1);
            OutgoingEdge in_order_dispatch(dispatch_vertex, 0);
            //cout << "In-order dispatch" << endl;
            addEdge(prev_dispatch_vertex, in_order_dispatch);
        }

        // In-order commit
        if (!no_need_for_ino_commit)
        {
            Vertex prev_commit_vertex(VertexType::InstrCommit, instrCount - 1);
            OutgoingEdge in_order_commit(commit_vertex, 0);
            //cout << "In-order commit" << endl;
            addEdge(prev_commit_vertex, in_order_commit);
        }
    }

    // Limited instuction buffer size
    if (instrCount >= instrBufferSize)
    {
        Vertex prev_commit_vertex(VertexType::InstrCommit, instrCount - instrBufferSize);
        OutgoingEdge limited_instr_buffer(fetch_vertex, 0);
        //cout << "Limited instruction buffer size" << endl;
        addEdge(prev_commit_vertex, limited_instr_buffer);
    }

    // Limited instruction queue size (alternative modeling approach)
    /*
    if (instrCount >= instrQueueSize)
    {
        Vertex prev_execute_vertex(VertexType::InstrExecute, instrCount - instrQueueSize);
        OutgoingEdge limited_instr_queue(dispatch_vertex, 0);
        //cout << "Limited instruction queue size" << endl;
        addEdge(prev_execute_vertex, limited_instr_queue);
    }
    */
}

bool O3CoreGraphAdvanced::modelMemoryOrderConstraint(Instruction* instr, Vertex& mem_vertex)
{
    bool store_to_load_forwarding = false;
    uint64_t base;
    uint32_t length;
    uint32_t index;
    bool is_load = (instr->memLoadCount != 0);
    uint32_t lq_size = scoreboard[0].getQueueSize(QueueResource::RscLQ);
    uint32_t sq_size = scoreboard[0].getQueueSize(QueueResource::RscSQ);

    if (is_load)
    {
        base = instr->memLoadBase;
        length = instr->memLoadLength;
    }
    else
    {
        base = instr->memStoreBase;
        length = instr->memStoreLength;
    }

    // Limited memory issue bandwidth
    index = (ldStWindowPointer + lq_size + sq_size - memIssueBandwidth) % (lq_size + sq_size);
    uint64_t previous_ld_st_num = ldStWindow[index].first;
    if (previous_ld_st_num != UINT64_MAX)
    {
        Vertex prev_mem_vertex(VertexType::MemExecute, previous_ld_st_num);
        OutgoingEdge limited_mem_issue_bw(mem_vertex, 0);
        //cout << "Limited memory issue bandwidth: " << previous_ld_st_num
        //     << " to " << instrCount << endl;
        addEdge(prev_mem_vertex, limited_mem_issue_bw);
    }

    // The following loop starts from the youngest load/store before this insturction.
    // If this instruction is a load, it checks if a store-to-load forwarding edge is needed.
    // If this instruction is a store, it checks if an edge from the youngest load/store
    // with a common address is needed.
    index = (ldStWindowPointer + lq_size + sq_size - 1) % (lq_size + sq_size);
    for (uint32_t i = 0; i < lq_size + sq_size; ++i)
    {
        uint64_t previous_ld_st_num = ldStWindow[index].first;
        if (previous_ld_st_num == UINT64_MAX)
        {
            break;
        }

        pair<uint64_t, uint32_t> previous_ld_st_addr = ldStWindow[index].second;
        uint64_t prev_base = previous_ld_st_addr.first;
        uint64_t prev_length = previous_ld_st_addr.second;
        bool is_prev_load = ldStWindowType[index];

        bool common = ((base >= prev_base) && (base < prev_base + prev_length)) ||
                      ((base + length > prev_base) && (base + length <= prev_base + prev_length));

        bool needs_edge = false;

        if (common && !is_prev_load && is_load)
        {
            needs_edge = true;
            store_to_load_forwarding = true;
            //cout << "Store to load forwarding: " << previous_ld_st_num
            //     << " to " << instrCount << endl;
        }

        if (common && !is_load)
        {
            needs_edge = true;
            //cout << "Store after load/store from/to same address: " << previous_ld_st_num
            //     << " to " << instrCount << endl;
        }

        if (needs_edge)
        {
                Vertex prev_mem_vertex(VertexType::MemExecute, previous_ld_st_num);
                OutgoingEdge limited_mem(mem_vertex, 0);
                addEdge(prev_mem_vertex, limited_mem);
                break;
        }

        index = (index + lq_size + sq_size - 1) % (lq_size + sq_size);
    }

    /*
    if (is_load)
    {
        // Additional constraint: load requests are only sent when they are non-speculative
        if ((lastBranch != UINT64_MAX) && (instrCount - lastBranch < instrBufferSize))
        {
            Vertex prev_br_vertex(VertexType::InstrCommit, lastBranch);
            OutgoingEdge non_speculative_load(mem_vertex, 0);
            addEdge(prev_br_vertex, non_speculative_load);
        }

        // Additional constraint: load requests are only sent when they are at ROB head
        if (instrCount > 0)
        {
            Vertex prev_instr_vertex(VertexType::InstrCommit, instrCount - 1);
            OutgoingEdge head_load(mem_vertex, 0);
            addEdge(prev_instr_vertex, head_load);
        }
    }
    */

    return store_to_load_forwarding;
}

void O3CoreGraphAdvanced::trackDataDependencies(Instruction* instr,
                                                Vertex& execute_vertex, Vertex& mem_vertex)
{
    // Check for data dependence through registers
    for (uint32_t i = 0; i < instr->regReadCount; ++i)
    {
        int reg_read = instr->regRead[i];
        if ((regLastWrittenBy.count(reg_read) != 0) &&
            (instrCount - regLastWrittenBy[reg_read].first < instrBufferSize))
        {
            //cout << "Register data dependence: " << instrCount << " to "
            //     << regLastWrittenBy[reg_read].first << endl;
            uint32_t weight; // Not differentiating between address and value registers for stores
            weight = regLastWrittenBy[reg_read].second;
  
            if (regLastWrittenByLoad[reg_read])
            {
                Vertex prev_mem_vertex(VertexType::MemExecute,
                                       regLastWrittenBy[reg_read].first);
                OutgoingEdge dependence_edge(execute_vertex, (int64_t)weight);
                addEdge(prev_mem_vertex, dependence_edge);
            }
            else
            {
                Vertex prev_execute_vertex(VertexType::InstrExecute,
                                           regLastWrittenBy[reg_read].first);
                OutgoingEdge dependence_edge(execute_vertex, (int64_t)weight);
                addEdge(prev_execute_vertex, dependence_edge);
            }
        }
    }
}

pair<uint32_t, bool> O3CoreGraphAdvanced::modelResourceDependencies()
{
    sys_nanoseconds my_time = chrono::system_clock::now();
 
    uint64_t orderly_scheduled_instr_count_vec[VECTOR_WIDTH] = {};
    bool all_scheduled_vec[VECTOR_WIDTH] = {};

    for (uint32_t i = 0; i < VECTOR_WIDTH; ++i)
    {
        // All instructions before headScheduledInstr (in all configuration scenarios
        // represented by different vector elements) have already been scheduled. In
        // each scenario, try to schedule instructions (assign resources) until either
        // headScheduledInstr is scheduled or scheduleOrder[i] is empty.
        bool scheduled_enough = false;
        while (!scheduled_enough)
        {
            Vertex* parent1;
            Vertex* parent2;
            Vertex* parent3;
            OutgoingEdge* e1;
            OutgoingEdge* e2;
            OutgoingEdge* e3;
            uint32_t wait_cycles;
            uint64_t prev_instr;
            uint64_t curr_instr;

            // Schedule the instuction with the shortest critical path
            curr_instr = scheduleOrder[i].begin()->first;
            if (curr_instr - headScheduledInstr > instrBufferSize)
            {
                CALIPERS_ERROR("Impossible stride: " << headScheduledInstr
                               << ", " << curr_instr);
            }
            scheduleOrder[i].erase(scheduleOrder[i].begin());
            alreadyScheduled[i].emplace(curr_instr);
            if (maxSchedInstrNum[i] < curr_instr)
            {
                maxSchedInstrNum[i] = curr_instr;
            }

            Vertex child(VertexType::InstrExecute, curr_instr);

            // Limited issue bandwidth
            scoreboard[i].scheduleResource(Resource::RscIssue, curr_instr,
                                           prev_instr, wait_cycles);
            if (prev_instr != UINT64_MAX)
            {
                Vertex prev_execute_vertex(VertexType::InstrExecute, prev_instr);
                OutgoingEdge limited_issue_bw(child, (int64_t)wait_cycles, i);
                //cout << "Limited issue bandwidth: " << prev_instr
                //     << " to " << curr_instr << endl;
                addEdge(prev_execute_vertex, limited_issue_bw);

                parent1 = &prev_execute_vertex;
                e1 = &limited_issue_bw;
            }
            else
            {
                parent1 = NULL;
            }

            // Limited instruction queue size
            uint32_t execution_cycles = executionCycles[curr_instr % AnalysisWindow];
            scoreboard[i].scheduleQueue(QueueResource::RscInstrQ, curr_instr, execution_cycles,
                                        prev_instr, wait_cycles);
            if (prev_instr != UINT64_MAX)
            {
                Vertex prev_execute_vertex(VertexType::InstrExecute, prev_instr);
                OutgoingEdge limited_instr_queue(child, (int64_t)wait_cycles, i);
                //cout << "Limited instruction queue size: " << prev_instr
                //     << " to " << curr_instr << endl;
                addEdge(prev_execute_vertex, limited_instr_queue);

                parent2 = &prev_execute_vertex;
                e2 = &limited_instr_queue;
            }
            else
            {
                parent2 = NULL;
            }

            // Limited execution units
            int execution_type = executionType[curr_instr % AnalysisWindow];
            int operation_type;
            int lsq_type = -1;
            switch (execution_type)
            {
                case ExecutionType::IntBase:
                case ExecutionType::BranchCond:
                case ExecutionType::BranchUncond:
                    operation_type = Resource::RscIntAlu;
                    break;
                case ExecutionType::IntMul:
                    operation_type = Resource::RscIntMul;
                    break;
                case ExecutionType::IntDiv:
                    operation_type = Resource::RscIntDiv;
                    break;
                case ExecutionType::FpBase:
                    operation_type = Resource::RscFpAlu;
                    break;
                case ExecutionType::FpMul:
                    operation_type = Resource::RscFpMul;
                    break;
                case ExecutionType::FpDiv:
                    operation_type = Resource::RscFpDiv;
                    break;
                case ExecutionType::Load:
                    lsq_type = QueueResource::RscLQ;
                    operation_type = Resource::RscLsu;
                    break;
                case ExecutionType::Store:
                    lsq_type = QueueResource::RscSQ;
                    operation_type = Resource::RscLsu;
                    break;
                default:
                    operation_type = -1;
            }

            if (operation_type != -1)
            {
                scoreboard[i].scheduleResource(operation_type, curr_instr,
                                               prev_instr, wait_cycles);
                if ((prev_instr != UINT64_MAX) &&
                    (unsigned_diff(curr_instr, prev_instr) < instrBufferSize))
                {
                    Vertex prev_execute_vertex(VertexType::InstrExecute, prev_instr);
                    OutgoingEdge limited_resource(child, (int64_t)wait_cycles, i);
                    //cout << "Resource dependence: " << curr_instr
                    //     << " to " << prev_instr << endl;
                    addEdge(prev_execute_vertex, limited_resource);

                    parent3 = &prev_execute_vertex;
                    e3 = &limited_resource;
                }
                else
                {
                    parent3 = NULL;
                }
            }
            else
            {
                parent3 = NULL;
            }
            updateCriticalPath(i, parent1, e1, parent2, e2, parent3, e3);

            // TODO: Model structural hazards related to the limited pipeline
            // length of an execution unit.

            // Limited load/store queue size
            if (lsq_type != -1)
            {
                uint32_t ls_cycles = lsCycles[curr_instr % AnalysisWindow];
                scoreboard[i].scheduleQueue(lsq_type, curr_instr, ls_cycles,
                                            prev_instr, wait_cycles);
                if ((prev_instr != UINT64_MAX) &&
                    (unsigned_diff(curr_instr, prev_instr) < instrBufferSize))
                {
                    Vertex curr_mem_vertex(VertexType::MemExecute, curr_instr);
                    Vertex prev_mem_vertex(VertexType::MemExecute, prev_instr);
                    OutgoingEdge limited_lsq(curr_mem_vertex, (int64_t)wait_cycles, i);
                    //cout << "Limited load/store queue size: " << prev_instr
                    //     << " to " << curr_instr << endl;
                    addEdge(prev_mem_vertex, limited_lsq);

                    parent1 = &prev_mem_vertex;
                    e1 = &limited_lsq;
                }
                else
                {
                    parent1 = NULL;
                }
            }
            updateCriticalPath(i, parent1, e1, NULL, NULL, NULL, NULL);

            // Calculate how many consecutive instructions (starting from
            // headScheduledInstr) have been scheduled.
            if (curr_instr == headScheduledInstr)
            {
                scheduled_enough = true;
                uint64_t current_head = headScheduledInstr;
                auto it = alreadyScheduled[i].begin();
                do
                {
                    if (*it - current_head == 0)
                    {
                        ++current_head;
                        ++orderly_scheduled_instr_count_vec[i];
                    }
                    else
                    {
                        break;
                    }
                    ++it;
                } while (it != alreadyScheduled[i].end());

            }
            if (scheduleOrder[i].empty())
            {
                scheduled_enough = true;
                all_scheduled_vec[i] = true;
            }
        }
    }

    // Find the minimum of orderly_scheduled_instr_count_vec, so that
    // headScheduledInstr is incremented by that amount.
    uint64_t scheduled_instr_count = UINT64_MAX;
    bool all_scheduled = true;
    for (uint32_t i = 0; i < VECTOR_WIDTH; ++i)
    {
        if (scheduled_instr_count > orderly_scheduled_instr_count_vec[i])
        {
            scheduled_instr_count = orderly_scheduled_instr_count_vec[i];   
        }
        if (!all_scheduled_vec[i])
        {
            all_scheduled = false;
        }
    }

    for (uint32_t i = 0; i < VECTOR_WIDTH; ++i)
    {
        for (uint64_t j = 0; j < scheduled_instr_count; ++j)
        {
            alreadyScheduled[i].erase(alreadyScheduled[i].begin());
        }
    }

    headScheduledInstr += scheduled_instr_count;
    //cout << "headScheduledInstr: " << headScheduledInstr << endl;

    // If headScheduledInstr has got too close to headInstr, increment
    // headInstr and read new instructions from the trace.
    uint32_t read_new;
    if (headInstr - headScheduledInstr < AnalysisWindow / 4)
    {
        read_new = AnalysisWindow / 4;
    }
    else
    {
        read_new = 0;
    }

    // Note that AnalysisWindow should be large enough so that the
    // instructions that are to be read replace the instructions whose
    // distance from headScheduledInstr is larger that instrBufferSize
    // (i.e., the instructions whose information like executionCycles
    // is not needed anymore).
    for (uint32_t i = 0; i < read_new; ++i)
    {
        for (int j = 0; j <= VertexType::Last; ++j)
        {
            Vertex v(j, headInstr - AnalysisWindow + i);
            graphChildren[v].clear();
            graphParents[v].clear();
            // Note that the hash of i and i + AnalysisWindow
            // is the same for the maps indexed by a vertex.
        }
    }
    headInstr += read_new;
    //if (read_new != 0)
    //{
    //    cout << "headInstr: " << headInstr << endl;
    //}

    graphAnalysisTime += (chrono::system_clock::now() - my_time).count();

    return pair<uint32_t, bool>(read_new, all_scheduled);
}

void O3CoreGraphAdvanced::addEdge(Vertex& parent, OutgoingEdge& e)
{
    //printEdge(parent, e);

    graphChildren[parent].push_back(e);
    Vertex& child = e.child;
    IncomingEdge to_child_edge(parent, e.weight);
    graphParents[child].push_back(to_child_edge);
}

void O3CoreGraphAdvanced::calculateInstructionCriticalPath()
{
    for (int i = 0; i <= VertexType::Last; ++i)
    {
        Vertex child(i, instrCount);
        for (uint32_t j = 0; j < graphParents[child].size(); ++j)
        {
            Vertex& parent = graphParents[child][j].parent;
            OutgoingEdge e(child, graphParents[child][j].weight);
            updateCriticalPathCycles(parent, e);
        }
        if (i == VertexType::InstrExecute)
        {
            for (uint32_t k = 0; k < VECTOR_WIDTH; ++k)
            {
                scheduleOrder[k].emplace_hint(scheduleOrder[k].cend(),
                    pair<uint64_t, int64_t>(instrCount, length[child][k]));
            }
        }
    }
}

void O3CoreGraphAdvanced::updateCriticalPath(uint32_t idx,
                                             Vertex* parent1, OutgoingEdge* e1,
                                             Vertex* parent2, OutgoingEdge* e2,
                                             Vertex* parent3, OutgoingEdge* e3)
{
    if ((parent1 == NULL) && (parent2 == NULL) && (parent3 == NULL))
    {
        return;
    }

    Vertex child;
    int64_t prev_length;
    bool length_changed = false;

    if (parent1 != NULL)
    {
        child = e1->child; // Should be the same as e2->child or e3->child
        prev_length = length[child][idx];
        updateCriticalPathCycles(*parent1, *e1);
        if (prev_length != length[child][idx])
        {
            length_changed = true;
        }
    }
    if (parent2 != NULL)
    {
        child = e2->child; // Should be the same as e1->child or e3->child
        prev_length = length[child][idx];
        updateCriticalPathCycles(*parent2, *e2);
        if (prev_length != length[child][idx])
        {
            length_changed = true;
        }
    }
    if (parent3 != NULL)
    {
        child = e3->child; // Should be the same as e1->child or e2->child
        prev_length = length[child][idx];
        updateCriticalPathCycles(*parent3, *e3);
        if (prev_length != length[child][idx])
        {
            length_changed = true;
        }
    }

    if (length_changed)
    {
        // Approach 1:
        // Update the corresponding children and descendants in the scheduling list
        list<Vertex> update_list;
        update_list.push_back(child);
        while (!update_list.empty())
        {
            Vertex& current_parent = update_list.front();
            for (uint32_t i = 0; i < graphChildren[current_parent].size(); ++i)
            {
                OutgoingEdge& e = graphChildren[current_parent][i];
                Vertex& current_child = e.child;
                prev_length = length[current_child][idx];
                updateCriticalPathCycles(current_parent, e);
                if (prev_length != length[current_child][idx])
                {
                    if (current_child.type == VertexType::InstrExecute)
                    {
                        // Update current_child for scheduling:
                        auto it = scheduleOrder[idx].find(pair<uint64_t, int64_t>(
                            current_child.instrNum, prev_length));
                        if (it == scheduleOrder[idx].end())
                        {
                            //printEdge(current_parent, e);
                            //it = scheduleOrder[idx].begin();
                            //uint64_t count = 0;
                            //while ((count < 10) && (it != scheduleOrder[idx].end()))
                            //{
                            //    count << it->first << " " << it->second << endl;
                            //    ++it;
                            //    ++count;
                            //}

                            CALIPERS_ERROR(
                                "Child with outdated length not found in the scheduling list");
                        }
                        scheduleOrder[idx].erase(it);
                        scheduleOrder[idx].emplace(pair<uint64_t, int64_t>(
                            current_child.instrNum, length[current_child][idx]));
                    }
                    //if (current_child.type != VertexType::InstrCommit)
                    {
                        update_list.push_back(current_child);
                    }
                }
            }
            update_list.erase(update_list.begin());
        }

        /*
        // Approach 2:
        // Try to just update the necessary children and descendants in the scheduling list
        for (int i = VertexType::InstrExecute; i <= VertexType::MemExecute; ++i)
        {
            Vertex parent(i, child.instrNum);
            for (uint32_t j = 0; j < graphChildren[parent].size(); ++j)
            {
                OutgoingEdge& e = graphChildren[parent][j];
                updateCriticalPathCycles(parent, e);
            }
        }
        for (uint64_t i = child.instrNum; i < maxSchedInstrNum[idx]; ++i)
        {
            Vertex parent(VertexType::InstrCommit, i);
            for (uint32_t j = 0; j < graphChildren[parent].size(); ++j)
            {
                OutgoingEdge& e = graphChildren[parent][j];
                updateCriticalPathCycles(parent, e);
            }
        }
        */
    }
}
