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

#include "graph_util.h"
#include "inorder_core_graph.h"

InorderCoreGraph::InorderCoreGraph(string trace_file_name,
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
                                   bool load_early_issue) :
                                   Graph(trace_file_name, result_file_name, instr_stream),
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
                                   memCommitBandwidth(mem_commit_bandwidth),
                                   maxMemAccesses(max_mem_accesses),
                                   loadDependentEarlyIssue(load_dependent_early_issue),
                                   loadEarlyIssue(load_early_issue)
{
    // TODO: Parameterize the last three arguments of initResource
    // (i.e., total_cycles, source_independent_cycles, next_issue_cycles)
    scoreboard.initResource(Resource::RscIntAlu, int_alu_count, 3, 2, 1);
    scoreboard.initResource(Resource::RscIntMul, int_mul_count, 3, 0, 1);
    scoreboard.initResource(Resource::RscIntDiv, int_div_count, 9, 0, 9);
    scoreboard.initResource(Resource::RscFpu, fpu_count, 6, 2, 1);
    scoreboard.initResource(Resource::RscLsu, lsu_count, 1, 1, 1);

    intAluTotalCycles = scoreboard.resourceTotalCycles(Resource::RscIntAlu);

    extraLoadLatency = 2; // TODO: Make this a parameter
    
    ldStWindow = new pair<uint64_t, uint32_t>[maxMemAccesses];

    initBookKeeping();
}

InorderCoreGraph::~InorderCoreGraph()
{
    delete[] ldStWindow;
}

void InorderCoreGraph::run()
{
    CALIPERS_INFO("Running the graph-based modeler...");

    sys_nanoseconds my_time;

    while (true)
    {
        my_time = chrono::system_clock::now();
        Instruction* instr = instrStream->next();
        streamTime += (chrono::system_clock::now() - my_time).count();

        if (instr == NULL)
        {
            break;
        }

        model(instr);
        ++instrCount;

        if (instrCount % 100000 == 0)
        {
            CALIPERS_INFO("*** " << instrCount << " instructions modeled/analyzed" << endl);
        }
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

void InorderCoreGraph::initBookKeeping()
{
    lastMisprediction = UINT64_MAX;
    previousInstrMispredicted = false;
    ldStWindowPointer = 0;
    lastMemLdSt = UINT64_MAX;
    lastLdStCriticalNum = UINT64_MAX;
    lastLdStCriticalCycles = UINT32_MAX;

    regLastWrittenBy.clear();
    regLastWrittenByLoad.clear();

    scoreboard.initRecords();

    for (uint32_t i = 0; i < AnalysisWindow; ++i)
    {
        neededRsc[i].first = -1;
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

    for (int i = 0; i <= VertexType::Last; ++i)
    {
        parents[i] = 0;
    }

    for (uint32_t i = 0; i < AnalysisWindow; ++i)
    {
        executionType[i] = -1;
    }

    for (uint32_t i = 0; i < maxMemAccesses; ++i)
    {
        ldStWindow[i].second = UINT32_MAX;
    }
}

void InorderCoreGraph::model(Instruction* instr)
{
    sys_nanoseconds my_time = chrono::system_clock::now();

    Vertex fetch_vertex(VertexType::InstrFetch, instrCount);
    Vertex dispatch_vertex(VertexType::InstrDispatch, instrCount);
    Vertex execute_vertex(VertexType::InstrExecute, instrCount);
    Vertex mem_vertex(VertexType::MemExecute, instrCount);
    Vertex commit_vertex(VertexType::InstrCommit, instrCount);

    uint32_t execution_cycles;
    uint32_t source_independent_cycles;

    bool is_load = (instr->memLoadCount == 1); // Also covers atomic instructions
    bool is_store = (instr->memStoreCount == 1); // Also covers atomic instructions
    bool is_load_store = is_load || is_store;
    bool is_branch = (instr->executionType == ExecutionType::BranchCond) || 
                     (instr->executionType == ExecutionType::BranchUncond);
    bool is_int = (instr->executionType == ExecutionType::IntBase) || is_branch;
    bool is_int_mul = (instr->executionType == ExecutionType::IntMul);
    bool is_int_div = (instr->executionType == ExecutionType::IntDiv);
    bool is_fp = (instr->executionType == ExecutionType::FpBase) ||
                 (instr->executionType == ExecutionType::FpMul) ||
                 (instr->executionType == ExecutionType::FpDiv);

    // Keeping track of current execute_vertex's dependence to previous execute_vertex'es
    unordered_map<uint64_t, uint32_t> execute_parent; // Key: Instruction num, Value: Weight

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
    else if (is_fp)
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
        execution_cycles = scoreboard.resourceTotalCycles(Resource::RscLsu) + instr->lsCycles;
        source_independent_cycles =
            scoreboard.resourceSourceIndependentCycles(Resource::RscLsu);
    }
    else if (is_int)
    {
        execution_cycles = scoreboard.resourceTotalCycles(Resource::RscIntAlu);
        source_independent_cycles =
            scoreboard.resourceSourceIndependentCycles(Resource::RscIntAlu);
    }
    else if (is_int_mul)
    {
        execution_cycles = scoreboard.resourceTotalCycles(Resource::RscIntMul);
        source_independent_cycles = 
            scoreboard.resourceSourceIndependentCycles(Resource::RscIntMul);
    }
    else if (is_int_div)
    {
        execution_cycles = scoreboard.resourceTotalCycles(Resource::RscIntDiv);
        source_independent_cycles =
            scoreboard.resourceSourceIndependentCycles(Resource::RscIntDiv);
    }
    else if (is_fp)
    {
        execution_cycles = scoreboard.resourceTotalCycles(Resource::RscFpu);
        source_independent_cycles =
            scoreboard.resourceSourceIndependentCycles(Resource::RscFpu);
    }
    else
    {
        execution_cycles = 1;
        source_independent_cycles = 0;
    }

    modelPipeline(fetch_vertex, dispatch_vertex,
                  execute_vertex, mem_vertex, commit_vertex,
                  instr, execution_cycles, execute_parent);

    if (is_load_store)
    {
        modelMemoryOrderConstraint(mem_vertex, is_load, is_store);
    }

    trackDataDependencies(instr, source_independent_cycles,
                          execute_vertex, execute_parent);

    modelResourceDependenciesSimple(is_int, is_int_mul, is_int_div, is_fp, is_load_store,
                                    execute_vertex, execute_parent);

    // Add the required InstrExecute to InstrExecute vertices
    for (auto i = execute_parent.begin(); i != execute_parent.end(); ++i)
    {
        if ((instrCount - i->first) / issueBandwidth < i->second)
        {
            Vertex prev_execute_vertex(VertexType::InstrExecute, i->first);
            OutgoingEdge dependence_edge(execute_vertex, (int64_t)i->second);
            addEdge(prev_execute_vertex, dependence_edge);

            if ((neededRsc[instrCount % AnalysisWindow].first != -1) &&
                (neededRsc[instrCount % AnalysisWindow].first ==
                 neededRsc[i->first % AnalysisWindow].first) &&
                (neededRsc[instrCount % AnalysisWindow].second ==
                 neededRsc[i->first % AnalysisWindow].second))
            {
                //cout << "Maintaining resource pipeline distance: " << instrCount
                //     << " to " << i->first << endl;
                Vertex prev_commit_vertex(VertexType::InstrCommit, i->first);
                OutgoingEdge dependence_edge(commit_vertex, (int64_t)i->second);
                addEdge(prev_commit_vertex, dependence_edge);
            }
        }
    }

    graphConstructionTime += (chrono::system_clock::now() - my_time).count();
    my_time = chrono::system_clock::now();

    calculateInstructionCriticalPath();


    // Update bookkeeping variables:

    if (is_load_store)
    {
        lastMemLdSt = instrCount;
        ldStWindow[ldStWindowPointer].first = instrCount;
        ldStWindow[ldStWindowPointer].second = instr->lsCycles;
        ldStWindowPointer = (ldStWindowPointer + 1) % maxMemAccesses;
    }

    if (!loadEarlyIssue && (is_int || is_int_mul || is_int_div || is_fp))
    {
        lastLdStCriticalNum = instrCount;
        lastLdStCriticalCycles = execution_cycles;

        //if (is_branch)
            //lastLdStCriticalCycles += predictionCycles / 2;
    }

    previousWasBranch = is_branch;
    linearPC = instr->pc + instr->bytes;

    for (uint32_t i = 0; i < instr->regWriteCount; ++i)
    {
        int reg_write = instr->regWrite[i];
        regLastWrittenBy[reg_write].first = instrCount;
        if (is_load)
        {
            if (loadDependentEarlyIssue)
            {
                regLastWrittenBy[reg_write].second = extraLoadLatency;
            }
            else
            {
                regLastWrittenBy[reg_write].second = instr->lsCycles;
            }
            regLastWrittenByLoad[reg_write] = true;
        }
        else
        {
            regLastWrittenBy[reg_write].second = execution_cycles;
            regLastWrittenByLoad[reg_write] = false;
        }
    }


    // Update miss statistics:

    if (instr->fetchCycles > l2iThreshold)
    {
        ++l2iMisses;
    }
    else if (instr->fetchCycles > l1iThreshold)
    {
        ++l1iMisses;
    }

    if (is_load_store)
    {
        if (instr->lsCycles > l2dThreshold)
        {
            ++l2dMisses;
        }
        else if (instr->lsCycles > l1dThreshold)
        {
            ++l1dMisses;
        }
    }

    if (is_branch)
    {
        ++branchCount;
        if (instr->mispredicted)
        {
            ++bpMisses;
        }
    }

    graphAnalysisTime += (chrono::system_clock::now() - my_time).count();
}

void InorderCoreGraph::modelPipeline(Vertex& fetch_vertex, Vertex& dispatch_vertex,
                                     Vertex& execute_vertex, Vertex& mem_vertex,
                                     Vertex& commit_vertex, Instruction* instr,
                                     uint32_t execution_cycles,
                                     unordered_map<uint64_t, uint32_t>& execute_parent)
{
    bool mispredicted;
    uint32_t fetch_cycles = instr->fetchCycles;
    bool is_load_store = (instr->memLoadCount == 1) || (instr->memStoreCount == 1);

    bool no_need_for_ino_dispatch = (instrCount == 0) || (dispatchBandwidth == 1);
    bool no_need_for_ino_issue = (instrCount == 0) || (issueBandwidth == 1);
    bool no_need_for_ino_commit = (instrCount == 0) || (commitBandwidth == 1);

    mispredicted = previousInstrMispredicted;
    previousInstrMispredicted = instr->mispredicted;

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
                                     (int64_t)scoreboard.resourceTotalCycles(Resource::RscLsu));
        //cout << "Memory execute after instruction execute" << endl;
        addEdge(execute_vertex, mem_after_instr);

        // Commit after execute
        OutgoingEdge commit_after_execute(commit_vertex,
                                          (int64_t)(instr->lsCycles + executeToCommitCycles));
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

    // Limited issue bandwidth
    if ((instrCount >= issueBandwidth) &&
        ((lastMisprediction == UINT64_MAX) ||
         (instrCount - lastMisprediction > issueBandwidth)))
    {
        Vertex prev_execute_vertex(VertexType::InstrExecute, instrCount - issueBandwidth);
        OutgoingEdge limited_issue_bw(execute_vertex, 1);
        //cout << "Limited issue bandwidth" << endl;
        addEdge(prev_execute_vertex, limited_issue_bw);

        execute_parent[instrCount - issueBandwidth] = 1;
    }

    // Limited memory issue bandwidth
    if ((lastMemLdSt != UINT64_MAX) &&
        (instrCount - lastMemLdSt <= memIssueBandwidth) &&
        ((lastMisprediction == UINT64_MAX) ||
         (instrCount - lastMisprediction > memIssueBandwidth)))
    {
        if (execute_parent[lastMemLdSt] < 1)
        {
            Vertex prev_execute_vertex(VertexType::InstrExecute, lastMemLdSt);
            OutgoingEdge limited_mem_issue_bw(execute_vertex, 1);
            //cout << "Limited memory issue bandwidth" << endl;
            addEdge(prev_execute_vertex, limited_mem_issue_bw);

            execute_parent[lastMemLdSt] = 1;
            no_need_for_ino_issue = no_need_for_ino_issue || ((instrCount - lastMemLdSt) == 1);
        }
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
        (instrCount - lastMemLdSt <= memCommitBandwidth) &&
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
        //Vertex prev_branch_vertex(VertexType::InstrCommit, instrCount - 1);
        //OutgoingEdge mispredicted_fetch(fetch_vertex,
        //                                (int64_t)(mispredictionPenalty + fetch_cycles));
        Vertex prev_branch_vertex(VertexType::InstrExecute, instrCount - 1);
        OutgoingEdge mispredicted_fetch(fetch_vertex,
            (int64_t)(scoreboard.resourceTotalCycles(Resource::RscIntAlu) +
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
            if (previousWasBranch && (instr->pc != linearPC)) // Correctly taken branch
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

        // In-order issue
        if (!no_need_for_ino_issue)
        {
            Vertex prev_execute_vertex(VertexType::InstrExecute, instrCount - 1);
            OutgoingEdge in_order_issue(execute_vertex, 0);
            //cout << "In-order issue" << endl;
            addEdge(prev_execute_vertex, in_order_issue);
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
}

void InorderCoreGraph::modelMemoryOrderConstraint(Vertex& mem_vertex, bool is_load, bool is_store)
{
    // For loads, add an edge from the earliest load/store in the load/store window
    if (is_load)
    {
        pair<uint64_t, uint32_t> earliest_ld_st = ldStWindow[ldStWindowPointer % maxMemAccesses];
        uint64_t earliest_ld_st_num = earliest_ld_st.first;
        uint32_t earliest_ld_st_cycles = earliest_ld_st.second;
        if (earliest_ld_st_cycles != UINT32_MAX)
        {
            Vertex prev_mem_vertex(VertexType::MemExecute, earliest_ld_st_num);
            OutgoingEdge limited_mem(mem_vertex, (int64_t)earliest_ld_st_cycles);
            //cout << "Maximum memory access constraint: load " 
            //     << instrCount << " to load/store " << earliest_ld_st_num << endl; 
            addEdge(prev_mem_vertex, limited_mem);
        }
    }

    // For stores, add an edge from all loads/stores in the load/store window
    if (is_store)
    {
        for (uint32_t i = 0; i < maxMemAccesses; ++i)
        {
            pair<uint64_t, uint32_t> previous_ld_st = ldStWindow[i];
            uint64_t previous_ld_st_num = previous_ld_st.first;
            uint32_t previous_ld_st_cycles = previous_ld_st.second;
            if (previous_ld_st_cycles != UINT32_MAX)
            {
                Vertex prev_mem_vertex(VertexType::MemExecute, previous_ld_st_num);
                OutgoingEdge limited_store(mem_vertex, (int64_t)previous_ld_st_cycles);
                //cout << "Store order constraint: store "
                //     << instrCount << " to load/store " << previous_ld_st_num << endl; 
                addEdge(prev_mem_vertex, limited_store);
            }
        }
    }

    // It seems that in gem5's MinorCPU, a load/store is never sent to the LSQ
    // before any previous instruction is completed in its functional unit pipeline.
    if ((lastLdStCriticalNum != UINT64_MAX) &&
        ((instrCount - lastLdStCriticalNum) / issueBandwidth < lastLdStCriticalCycles))
    {
        Vertex prev_mem_critical_vertex(VertexType::InstrExecute, lastLdStCriticalNum);
        OutgoingEdge mem_wait(mem_vertex, (int64_t)lastLdStCriticalCycles);
        //Vertex prev_mem_critical_vertex(VertexType::InstrCommit, lastLdStCriticalNum);
        //OutgoingEdge mem_wait(mem_vertex, 0);
        //cout << "Load/store " << instrCount << " must be sent to memory after completion of "
        //     << lastLdStCriticalNum << endl;
        addEdge(prev_mem_critical_vertex, mem_wait);
    }
}

void InorderCoreGraph::trackDataDependencies(Instruction* instr,
                                             uint32_t source_independent_cycles,
                                             Vertex& execute_vertex,
                                             unordered_map<uint64_t, uint32_t>& execute_parent)
{
    // Check for data dependence through registers
    for (uint32_t i = 0; i < instr->regReadCount; ++i)
    {
        int reg_read = instr->regRead[i];
        if (regLastWrittenBy.count(reg_read) != 0)
        {
            //cout << "Register data dependence: " << instrCount << " to "
            //     << regLastWrittenBy[reg_read].first << endl;
            uint32_t weight; // Not differentiating between address and value registers for stores
            if (regLastWrittenByLoad[reg_read])
            {
                weight = regLastWrittenBy[reg_read].second;
                if (loadDependentEarlyIssue)
                {
                    if (weight > source_independent_cycles)
                    {
                        weight -= source_independent_cycles;
                    }
                    else
                    {
                        weight = 0;
                    }

                    if ((instrCount - regLastWrittenBy[reg_read].first) / issueBandwidth < weight)
                    {
                        Vertex prev_mem_vertex(VertexType::MemExecute,
                                               regLastWrittenBy[reg_read].first);
                        OutgoingEdge dependence_edge(execute_vertex, (int64_t)weight);
                        addEdge(prev_mem_vertex, dependence_edge);
                    }
                }
                else
                {
                    if ((instrCount - regLastWrittenBy[reg_read].first) / issueBandwidth < weight)
                    {
                        Vertex prev_commit_vertex(VertexType::InstrCommit,
                                                  regLastWrittenBy[reg_read].first);
                        OutgoingEdge dependence_edge(execute_vertex, 0);
                        addEdge(prev_commit_vertex, dependence_edge);
                    }
                }
            }
            else
            {
                if (regLastWrittenBy[reg_read].second > source_independent_cycles)
                {
                    weight = regLastWrittenBy[reg_read].second - source_independent_cycles;
                }
                else
                {
                    weight = 0;
                }
                if (execute_parent[regLastWrittenBy[reg_read].first] < weight)
                {
                    execute_parent[regLastWrittenBy[reg_read].first] = weight;
                }
            }
        }
    }
}

// This is called "simple" because resource instances are assigned to instructions
// in program order. Critical path information can be used in a more complex model,
// where resource assignment can be done using the LRU method, i.e., the resource
// instance which has the shortest critical path to its youngest user's execute_vertex
// is chosen to be assigned to the current instruction. Moreover, stalls can be
// detected using critical path information, which can be used to more accurately
// model the structural hazard related to the limited pipeline length of a resource.
// However, in our experiments, we found that this more complex model just slightly
// improves the accuracy (with gem5 as the baseline).
void InorderCoreGraph::modelResourceDependenciesSimple(
                          bool is_int, bool is_int_mul,
                          bool is_int_div, bool is_fp,
                          bool is_load_store, Vertex& execute_vertex,
                          unordered_map<uint64_t, uint32_t>& execute_parent)
{
    int type;
    if (is_int)
    {
        type = Resource::RscIntAlu;
    }
    else if (is_int_mul)
    {
        type = Resource::RscIntMul;
    }
    else if (is_int_div)
    {
        type = Resource::RscIntDiv;
    }
    else if (is_fp)
    {
        type = Resource::RscFpu;
    }
    else if (is_load_store)
    {
        type = Resource::RscLsu;
    }
    else
    {
        neededRsc[instrCount % AnalysisWindow].first = -1;
        return;
    }

    uint32_t instance;
    uint64_t previous_instr;
    uint32_t wait_cycles;
    uint64_t head_of_pipeline;
    scoreboard.scheduleResource(type, instrCount,
                                instance, previous_instr,
                                wait_cycles, head_of_pipeline);
    neededRsc[instrCount % AnalysisWindow].first = type;
    neededRsc[instrCount % AnalysisWindow].second = instance;

    if (previous_instr != UINT64_MAX)
    {
        //cout << "Resource dependence of " << instrCount
        //     << " to " << previous_instr << endl;
        if (execute_parent[previous_instr] < wait_cycles)
            execute_parent[previous_instr] = wait_cycles;
    }

    if ((head_of_pipeline != UINT64_MAX) &&
        (instrCount - head_of_pipeline < AnalysisWindow))
    {
        //cout << "Limited pipeline length dependence: " << instrCount 
        //     << " to " << head_of_pipeline << endl;
        if (is_load_store)
        {
            Vertex prev_execute_vertex(VertexType::MemExecute, head_of_pipeline);
            OutgoingEdge pipeline_limit(execute_vertex, 0);
            addEdge(prev_execute_vertex, pipeline_limit);
        }
        else
        {
            Vertex prev_commit_vertex(VertexType::InstrCommit, head_of_pipeline);
            OutgoingEdge pipeline_limit(execute_vertex, 0);
            addEdge(prev_commit_vertex, pipeline_limit);
        }
    }

    if (is_load_store)
    {
        neededRsc[instrCount % AnalysisWindow].first = -1; // Will take care of this later
    }
}

void InorderCoreGraph::addEdge(Vertex& parent, OutgoingEdge& e)
{
    // It may have been better to define this function with a
    // child vertex and an incoming edge. But in an earlier version
    // of addEdge, it was like this, and I left it this way.

    //printEdge(parent, e);

    int child_type = e.child.type;
    int parents_count = parents[child_type];

    if (e.child.instrNum - parent.instrNum > AnalysisWindow)
    {
        printEdge(parent, e);
        CALIPERS_ERROR("The parent-child distance exceeds the window size");
    }

    if (parents_count == MAX_PARENTS)
    {
        printEdge(parent, e);
        CALIPERS_ERROR("The vertex has the maximum number of parents");
    }
    
    miniGraph[child_type][parents_count].parent = parent;
    miniGraph[child_type][parents_count].weight = e.weight;
    ++parents_count;
    parents[child_type] = parents_count;
}

void InorderCoreGraph::calculateInstructionCriticalPath()
{
    for (int i = 0; i <= VertexType::Last; ++i)
    {
        Vertex child(i, instrCount);
        for (uint32_t j = 0; j < parents[i]; ++j)
        {
            Vertex& parent = miniGraph[i][j].parent;
            OutgoingEdge e(child, miniGraph[i][j].weight);
            updateCriticalPathCycles(parent, e);
        }
        parents[i] = 0;
    }
}
