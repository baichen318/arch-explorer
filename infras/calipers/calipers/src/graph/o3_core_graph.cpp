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

#include "calipers_defs.h"
#include "calipers_types.h"
#include "graph_util.h"
#include "o3_core_graph.h"
#include "cache.h"
#include "ideal_cache.h"
#include "statistical_cache.h"
#include "real_cache.h"
#include "branch_predictor.h"
#include "statistical_bp.h"
#include "visualizer.h"


O3CoreGraph::O3CoreGraph(string trace_file_name,
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

O3CoreGraph::~O3CoreGraph()
{
    delete[] ldStWindow;
    delete[] ldStWindowType;
}

void O3CoreGraph::run()
{
    CALIPERS_INFO("Running the graph-based modeler...");

    sys_nanoseconds my_time;

    while (true)
    {
        my_time = chrono::system_clock::now();
        Instruction* instr = instrStream->next();
        streamTime += (chrono::system_clock::now() - my_time).count();

        if ((instrCount > 0) && (instrCount % AnalysisWindow == 0))
        {
            anaylzeWindow();
            initBookKeeping();
        }

        if (instr == NULL)
        {
            if (instrCount % AnalysisWindow != 0)
            {
                anaylzeWindow();
            }
            break;
        }

        model(instr);
        ++instrCount;

        if (instrCount == 30) {
            auto visualizer = Visualizer(graph);
            visualizer.generate_dot("fig.dot");
            exit(EXIT_SUCCESS);
        }

        if (instrCount % 100000 == 0)
        {
            CALIPERS_INFO("*** " << instrCount << " instructions modeled" << endl);
        }
    }

    CALIPERS_INFO("Instruction stream time: "
                  << (streamTime / 1000000) << " ms" << endl);
    CALIPERS_INFO("Graph construction time: "
                  << (graphConstructionTime / 1000000) << " ms" << endl);
    CALIPERS_INFO("Graph analysis time:     "
                  << (graphAnalysisTime / 1000000) << " ms" << endl);
}

void O3CoreGraph::initBookKeeping()
{
    currentIcacheLine = UINT64_MAX;
    lastMisprediction = UINT64_MAX;
    lastBranch = UINT64_MAX;
    previousInstrMispredicted = false;
    previousWasBranch = false;
    lastMemLdSt = UINT64_MAX;
    ldStWindowPointer = 0;

    regLastWrittenBy.clear();
    regLastWrittenByLoad.clear();

    uint32_t ld_st_window_size = scoreboard[0].getQueueSize(QueueResource::RscLQ) +
                                 scoreboard[0].getQueueSize(QueueResource::RscSQ);
    for (uint32_t i = 0; i < ld_st_window_size; ++i)
    {
        ldStWindow[i].first = UINT64_MAX;
    }

    // Note that the hash of i and i + AnalysisWindow is the same for 
    // the maps indexed by a vertex.
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

    graph.clear();

    for (uint32_t i = 0; i < AnalysisWindow; ++i)
    {
        executionType[i] = -1;
        lsCycles[i] = UINT32_MAX;
        executionCycles[i] = UINT32_MAX;
    }

    for (uint32_t i = 0; i < VECTOR_WIDTH; ++i)
    {
        //scoreboard[i].resetResource(Resource::RscFetch);
        //scoreboard[i].resetResource(Resource::RscDispatch);
        scoreboard[i].resetResource(Resource::RscIssue);
        //scoreboard[i].resetResource(Resource::RscMemIssue);
        //scoreboard[i].resetResource(Resource::RscCommit);
        //scoreboard[i].resetResource(Resource::RscMemCommit);
        scoreboard[i].resetResource(Resource::RscIntAlu);
        scoreboard[i].resetResource(Resource::RscIntMulDiv);
        scoreboard[i].resetResource(Resource::RscFpAlu);
        scoreboard[i].resetResource(Resource::RscFpMulDiv);
        scoreboard[i].resetResource(Resource::RscLsu);

        scoreboard[i].resetQueue(QueueResource::RscInstrQ);
        scoreboard[i].resetQueue(QueueResource::RscLQ);
        scoreboard[i].resetQueue(QueueResource::RscSQ);
    }
}

void O3CoreGraph::anaylzeWindow()
{
    sys_nanoseconds my_time = chrono::system_clock::now();

    calculateCriticalPathForScheduling();
    //recordStats(false, true);
    modelResourceDependencies();
    calculateFinalCriticalPath();

    // visualize a graph here
    auto visualizer = Visualizer(graph);
    visualizer.generate_dot("fig.dot");

    recordStats(true, true);

    ++analyzedWindows;

    graphAnalysisTime += (chrono::system_clock::now() - my_time).count();
}

void O3CoreGraph::model(Instruction* instr)
{
    sys_nanoseconds my_time = chrono::system_clock::now();

    Vertex fetch_vertex(VertexType::InstrFetch, instrCount, instr->inst);
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


    // Update bookkeeping variables:

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

void O3CoreGraph::modelPipeline(Vertex& fetch_vertex, Vertex& dispatch_vertex,
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
    bool no_need_for_ino_dispatch = (instrCount % AnalysisWindow == 0) ||
                                    (dispatchBandwidth == 1);
    bool no_need_for_ino_commit = (instrCount % AnalysisWindow == 0) || 
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
    // if this condition is satisfied, we add an edge 'F_i => F_instrCount'.
    if ((instrCount % AnalysisWindow >= fetchBandwidth) &&
        ((lastMisprediction == UINT64_MAX) ||
         (instrCount - lastMisprediction > fetchBandwidth)))
    {
        Vertex prev_fetch_vertex(VertexType::InstrFetch, instrCount - fetchBandwidth);
        OutgoingEdge limited_fetch_bw(fetch_vertex, 1);
        //cout << "Limited fetch bandwidth" << endl;
        addEdge(prev_fetch_vertex, limited_fetch_bw);
    }

    // Limited dispatch bandwidth
    // if this condition is satisfied, we addd an edge 'D_i => D_instrCount'.
    if ((instrCount % AnalysisWindow >= dispatchBandwidth) &&
        ((lastMisprediction == UINT64_MAX) ||
         (instrCount - lastMisprediction > dispatchBandwidth)))
    {
        Vertex prev_dispatch_vertex(VertexType::InstrDispatch, instrCount - dispatchBandwidth);
        OutgoingEdge limited_dispatch_bw(dispatch_vertex, 1);
        //cout << "Limited dispatch bandwidth" << endl;
        addEdge(prev_dispatch_vertex, limited_dispatch_bw);
    }

    // Limited commit bandwidth
    // if this condition is satisfied, we addd an edge 'C_i => C_instrCount'.
    if ((instrCount % AnalysisWindow >= commitBandwidth) &&
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
        // if we have a misprediction, then we add an edge 'E_{instrCount - 1} => F_instrCount'.
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
        if (instrCount % AnalysisWindow != 0)
        {
            // if it is a normal fetch, we add an edge 'F_{instrCount - 1} => F_{instrCount}'.
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
            // if it is an in-order dispatch, we add an edge 'D_{instrCount - 1} => D_{instrCount}'.
            Vertex prev_dispatch_vertex(VertexType::InstrDispatch, instrCount - 1);
            OutgoingEdge in_order_dispatch(dispatch_vertex, 0);
            //cout << "In-order dispatch" << endl;
            addEdge(prev_dispatch_vertex, in_order_dispatch);
        }

        // In-order commit
        if (!no_need_for_ino_commit)
        {
            // if it is an in-order commit, we add an edge 'C_{instrCount - 1} => C_{instrCount}'.
            Vertex prev_commit_vertex(VertexType::InstrCommit, instrCount - 1);
            OutgoingEdge in_order_commit(commit_vertex, 0);
            //cout << "In-order commit" << endl;
            addEdge(prev_commit_vertex, in_order_commit);
        }
    }

    // Limited instuction buffer size
    if (instrCount % AnalysisWindow >= instrBufferSize)
    {
        // if it surpasses the instruction buffer size, we add an edge 'C_{instrCount - instrBufferSize} => F_{instrCount}'
        Vertex prev_commit_vertex(VertexType::InstrCommit, instrCount - instrBufferSize);
        OutgoingEdge limited_instr_buffer(fetch_vertex, 0);
        //cout << "Limited instruction buffer size" << endl;
        addEdge(prev_commit_vertex, limited_instr_buffer);
    }

    // Limited instruction queue size (alternative modeling approach)
    /*
    if (instrCount % AnalysisWindow >= instrQueueSize)
    {
        Vertex prev_execute_vertex(VertexType::InstrExecute, instrCount - instrQueueSize);
        OutgoingEdge limited_instr_queue(dispatch_vertex, 0);
        //cout << "Limited instruction queue size" << endl;
        addEdge(prev_execute_vertex, limited_instr_queue);
    }
    */
}

bool O3CoreGraph::modelMemoryOrderConstraint(Instruction* instr, Vertex& mem_vertex)
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

    // if the memory-related instructions (within memory issue bandwidth) still occupy `lq_size` or `sq_size`,
    // hence, `instr` shall be stalled, and we add an edge 'M_n => M_m' with a temporary weight: 0.
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
    // if these conditions are satisfied, then we add an 'M_n => M_m' edge with a temporary weight: 0.
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

        // common:
        //     |----------------------------------------------------|
        // prev_base                                      prev_base + prev_length
        //          |------------------------------------------|
        //         base                                   base + length
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

void O3CoreGraph::trackDataDependencies(Instruction* instr,
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
                // if this condition is satisfied, we add an edge 'M_{regLastWrittenBy[reg_read].first} => E_instrCount'.
                Vertex prev_mem_vertex(VertexType::MemExecute,
                                       regLastWrittenBy[reg_read].first);
                OutgoingEdge dependence_edge(execute_vertex, (int64_t)weight);
                addEdge(prev_mem_vertex, dependence_edge);
            }
            else
            {
                // if this condition is satisfied, we add an edge 'E_{regLastWrittenBy[reg_read].first} => E_instrCount'.
                Vertex prev_execute_vertex(VertexType::InstrExecute,
                                           regLastWrittenBy[reg_read].first);
                OutgoingEdge dependence_edge(execute_vertex, (int64_t)weight);
                addEdge(prev_execute_vertex, dependence_edge);
            }
        }
    }
}

void O3CoreGraph::modelResourceDependencies()
{
    // Note: It is OK if there are more than one edge from vertex v1 to
    // vertex v2 even if the edges have positive weights in a scenario.
    // It is also OK if there is an edge from v1 to v2 and also an edge
    // from v2 to v1. But at least one of the corresponing weights in a
    // scenario must be -1; otherwise, a loop is formed.

    for (uint32_t i = 0; i < VECTOR_WIDTH; ++i)
    {
        for (auto j = scheduleOrder[i].begin(); j != scheduleOrder[i].end(); ++j)
        {
            uint64_t curr_instr = j->first;
            uint64_t prev_instr;
            uint32_t wait_cycles;

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
                    continue;
            }

            Vertex curr_execute_vertex(VertexType::InstrExecute, curr_instr);
    
            // Limited issue bandwidth
            scoreboard[i].scheduleResource(Resource::RscIssue, curr_instr,
                                           prev_instr, wait_cycles);
            if (prev_instr != UINT64_MAX)
            {
                // if this condition (control hazards in IQW) is satisfied, we add an edge 'E_{prev_instr} => E_{curr_instr}'.
                Vertex prev_execute_vertex(VertexType::InstrExecute, prev_instr);
                OutgoingEdge limited_issue_bw(curr_execute_vertex, (int64_t)wait_cycles, i);
                //cout << "Limited issue bandwidth: " << prev_instr
                //     << " to " << curr_instr << endl;
                addEdge(prev_execute_vertex, limited_issue_bw);
            }

            // Limited instruction queue size
            uint32_t execution_cycles = executionCycles[curr_instr % AnalysisWindow];
            if (execution_cycles == UINT32_MAX)
            {
                CALIPERS_ERROR("Execution cycles not recorded properly");
            }
            scoreboard[i].scheduleQueue(QueueResource::RscInstrQ, curr_instr, execution_cycles,
                                        prev_instr, wait_cycles);
            if (prev_instr != UINT64_MAX)
            {
                // if this condition (control hazards in IQE) is satisfied, we add an edge 'E_{prev_instr} => E_{curr_instr}'.
                Vertex prev_execute_vertex(VertexType::InstrExecute, prev_instr);
                OutgoingEdge limited_instr_queue(curr_execute_vertex, (int64_t)wait_cycles, i);
                //cout << "Limited instruction queue size: " << prev_instr
                //     << " to " << curr_instr << endl;
                addEdge(prev_execute_vertex, limited_instr_queue);
            }

            // Limited execution units
            scoreboard[i].scheduleResource(operation_type, curr_instr,
                                           prev_instr, wait_cycles);

            if ((prev_instr != UINT64_MAX) &&
                (unsigned_diff(curr_instr, prev_instr) < instrBufferSize))
            {
                // if this condition (control hazards in EU) is satisfied, we add an edge 'E_{prev_instr} => E_{curr_instr}'.
                Vertex prev_execute_vertex(VertexType::InstrExecute, prev_instr);
                OutgoingEdge limited_resource(curr_execute_vertex, (int64_t)wait_cycles, i);
                //cout << "Resource dependence of " << curr_instr
                //     << " to " << prev_instr << endl;
                addEdge(prev_execute_vertex, limited_resource);
            }
            // TODO: Model structural hazards related to the limited pipeline
            // length of an execution unit.

            // Limited load/store queue size
            if (lsq_type != -1)
            {
                uint32_t ls_cycles = lsCycles[curr_instr % AnalysisWindow];
                if (ls_cycles == UINT32_MAX)
                {
                    CALIPERS_ERROR("Load/Store cycles not recorded properly");
                }
                scoreboard[i].scheduleQueue(lsq_type, curr_instr, ls_cycles,
                                            prev_instr, wait_cycles);
                if ((prev_instr != UINT64_MAX) &&
                    (unsigned_diff(curr_instr, prev_instr) < instrBufferSize))
                {
                    // if this condition (control hazards in LSU) is satisfied, we add an edge 'E_{prev_instr} => E_{curr_instr}'.
                    Vertex curr_mem_vertex(VertexType::MemExecute, curr_instr);
                    Vertex prev_mem_vertex(VertexType::MemExecute, prev_instr);
                    OutgoingEdge limited_lsq(curr_mem_vertex, (int64_t)wait_cycles, i);
                    //cout << "Limited load/store queue size: " << prev_instr
                    //     << " to " << curr_instr << endl;
                    addEdge(prev_mem_vertex, limited_lsq);
                }
            }
        }
    }
}

void O3CoreGraph::addEdge(Vertex& parent, OutgoingEdge& e)
{
    // printEdge(parent, e);

    graph[parent].push_back(e);

    graph[e.child]; // Just to be added to the graph if it is the last commit
}

void O3CoreGraph::calculateCriticalPathForScheduling()
{
    CALIPERS_INFO("Calculating critical path of window " << analyzedWindows 
                  << " for instrcution scheduling...");

    // These two for-loops traverse vertices in an obvious topological order
    for (uint64_t i = analyzedWindows * AnalysisWindow; i < instrCount; ++i)
    {
        for (int j = 0; j <= VertexType::Last; ++j)
        {
            Vertex parent(j, i);

            if (j == VertexType::InstrExecute)
            {
                for (uint32_t k = 0; k < VECTOR_WIDTH; ++k)
                {
                    scheduleOrder[k].emplace_hint(scheduleOrder[k].cend(),
                                                  pair<uint64_t, int64_t>(i, length[parent][k]));
                }
            }
            // It is also possible to consider a different order for MemExecute vertices.

            for (uint32_t k = 0; k < graph[parent].size(); ++k)
            {
                OutgoingEdge& e = graph[parent][k];
                updateCriticalPathCycles(parent, e);
            }
        }
    }
}

void O3CoreGraph::calculateFinalCriticalPath()
{
    CALIPERS_INFO("Calculating final critical path of window " << analyzedWindows << "...");

    /*
    // First, update critical path information of InstrExecute vertices
    for (uint32_t i = 0; i < VECTOR_WIDTH; ++i)
    {
        for (auto j = scheduleOrder[i].begin(); j != scheduleOrder[i].end(); ++j)
        {
            Vertex parent(VertexType::InstrExecute, j->first);
            for (uint32_t k = 0; k < graph[parent].size(); ++k)
            {
                OutgoingEdge& e = graph[parent][k];
                updateCriticalPathCycles(parent, e);
            }
        }
        scheduleOrder[i].clear();
    }

    // Then, update critical path information of MemExecute and InstrCommit vertices
    for (uint64_t i = analyzedWindows * AnalysisWindow; i < instrCount; ++i)
    {
        for (int j = VertexType::MemExecute; j <= VertexType::Last; ++j)
        {
            Vertex parent(j, i);

            for (uint32_t k = 0; k < graph[parent].size(); ++k)
            {
                OutgoingEdge& e = graph[parent][k];
                updateCriticalPathCycles(parent, e);
            }
        }
    }
    */

    // The following traversal may result in somewhat approximate updates.
    // BTW, what was the problem of the above traversal?
    for (uint32_t i = 0; i < VECTOR_WIDTH; ++i)
    {
        for (auto j = scheduleOrder[i].begin(); j != scheduleOrder[i].end(); ++j)
        {
            for (int k = VertexType::InstrExecute; k <= VertexType::Last; ++k)
            {
                Vertex parent(k, j->first);
                for (uint32_t l = 0; l < graph[parent].size(); ++l)
                {
                    OutgoingEdge& e = graph[parent][l];
                    updateCriticalPathCycles(parent, e);
                }
            }
        }
        scheduleOrder[i].clear();
    }
}
