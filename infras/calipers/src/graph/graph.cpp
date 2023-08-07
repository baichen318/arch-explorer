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


#include <iostream>
#include <iomanip>
#include <string>
#include <fstream>
#include <sstream>

#include "calipers_defs.h"
#include "calipers_types.h"
#include "graph.h"
#include "instruction_stream.h"

using namespace std;

Graph::Graph(
    string trace_file_name,
    string result_file_name,
    string view_dot_path,
    InstructionStream* instr_stream
) :
    streamTime(0),
    graphConstructionTime(0),
    graphAnalysisTime(0),
    instrCount(0),
    analyzedWindows(0),
    l1iMisses(0),
    l2iMisses(0),
    l1dMisses(0),
    l2dMisses(0),
    bpMisses(0),
    branchCount(0),
    traceFileName(trace_file_name),
    resultFileName(result_file_name),
    view_dot_path(view_dot_path),
    instrStream(instr_stream)
{
    view = (view_dot_path == "false") ? false : true;
    // TODO: Parameterize the following
    l1iThreshold = 5;
    l2iThreshold = 20;
    l1dThreshold = 5;
    l2dThreshold = 20;
}

void Graph::updateCriticalPathCycles(Vertex& parent, OutgoingEdge& e)
{
    bool mask[VECTOR_WIDTH];
    bool comparison[VECTOR_WIDTH];

    Vertex& child = e.child;
    Vector& weight = e.weight;

    // greedily update the critical path length: child, parent, and weight edge
    length[child].update(length[parent], weight, mask, VECTOR_WIDTH);

    criticalPathInstructions[child].intInstructions.maskedSet(
        criticalPathInstructions[parent].intInstructions, mask, VECTOR_WIDTH);
    criticalPathInstructions[child].fpInstructions.maskedSet(
        criticalPathInstructions[parent].fpInstructions, mask, VECTOR_WIDTH);
    criticalPathInstructions[child].loadInstructions.maskedSet(
        criticalPathInstructions[parent].loadInstructions, mask, VECTOR_WIDTH);
    criticalPathInstructions[child].storeInstructions.maskedSet(
        criticalPathInstructions[parent].storeInstructions, mask, VECTOR_WIDTH);
    criticalPathInstructions[child].branchInstructions.maskedSet(
        criticalPathInstructions[parent].branchInstructions, mask, VECTOR_WIDTH);
    criticalPathInstructions[child].otherInstructions.maskedSet(
        criticalPathInstructions[parent].otherInstructions, mask, VECTOR_WIDTH);

    criticalPathCycles[child].goodFetchHitCycles.maskedSet(
        criticalPathCycles[parent].goodFetchHitCycles, mask, VECTOR_WIDTH);
    criticalPathCycles[child].goodFetchMissCycles.maskedSet(
        criticalPathCycles[parent].goodFetchMissCycles, mask, VECTOR_WIDTH);
    criticalPathCycles[child].badFetchHitCycles.maskedSet(
        criticalPathCycles[parent].badFetchHitCycles, mask, VECTOR_WIDTH);
    criticalPathCycles[child].badFetchMissCycles.maskedSet(
        criticalPathCycles[parent].badFetchMissCycles, mask, VECTOR_WIDTH);
    criticalPathCycles[child].decodeCycles.maskedSet(
        criticalPathCycles[parent].decodeCycles, mask, VECTOR_WIDTH);
    criticalPathCycles[child].dispatchCycles.maskedSet(
        criticalPathCycles[parent].dispatchCycles, mask, VECTOR_WIDTH);
    criticalPathCycles[child].intCycles.maskedSet(
        criticalPathCycles[parent].intCycles, mask, VECTOR_WIDTH);
    criticalPathCycles[child].fpCycles.maskedSet(
        criticalPathCycles[parent].fpCycles, mask, VECTOR_WIDTH);
    criticalPathCycles[child].lsCycles.maskedSet(
        criticalPathCycles[parent].lsCycles, mask, VECTOR_WIDTH);
    criticalPathCycles[child].loadL1HitCycles.maskedSet(
        criticalPathCycles[parent].loadL1HitCycles, mask, VECTOR_WIDTH);
    criticalPathCycles[child].loadL2HitCycles.maskedSet(
        criticalPathCycles[parent].loadL2HitCycles, mask, VECTOR_WIDTH);
    criticalPathCycles[child].loadMissCycles.maskedSet(
        criticalPathCycles[parent].loadMissCycles, mask, VECTOR_WIDTH);
    criticalPathCycles[child].storeL1HitCycles.maskedSet(
        criticalPathCycles[parent].storeL1HitCycles, mask, VECTOR_WIDTH);
    criticalPathCycles[child].storeL2HitCycles.maskedSet(
        criticalPathCycles[parent].storeL2HitCycles, mask, VECTOR_WIDTH);
    criticalPathCycles[child].storeMissCycles.maskedSet(
        criticalPathCycles[parent].storeMissCycles, mask, VECTOR_WIDTH);
    criticalPathCycles[child].branchCycles.maskedSet(
        criticalPathCycles[parent].branchCycles, mask, VECTOR_WIDTH);
    criticalPathCycles[child].syscallCycles.maskedSet(
        criticalPathCycles[parent].syscallCycles, mask, VECTOR_WIDTH);
    criticalPathCycles[child].atomicCycles.maskedSet(
        criticalPathCycles[parent].atomicCycles, mask, VECTOR_WIDTH);
    criticalPathCycles[child].otherCycles.maskedSet(
        criticalPathCycles[parent].otherCycles, mask, VECTOR_WIDTH);
    criticalPathCycles[child].commitCycles.maskedSet(
        criticalPathCycles[parent].commitCycles, mask, VECTOR_WIDTH);

    if (((parent.type == VertexType::InstrExecute) && (child.type != VertexType::MemExecute)) ||
         (parent.type == VertexType::MemExecute))
    {
        int parent_execution_type = executionType[parent.instrNum % AnalysisWindow];
        Vector one_vector(1);
        switch (parent_execution_type)
        {
            case ExecutionType::IntBase:
            case ExecutionType::IntMul:
            case ExecutionType::IntDiv:
                criticalPathInstructions[child].intInstructions.maskedAdd(
                    one_vector, mask, VECTOR_WIDTH);
                break;
            case ExecutionType::FpBase:
            case ExecutionType::FpMul:
            case ExecutionType::FpDiv:
                criticalPathInstructions[child].fpInstructions.maskedAdd(
                    one_vector, mask, VECTOR_WIDTH);
                break;
            case ExecutionType::Load:
                criticalPathInstructions[child].loadInstructions.maskedAdd(
                    one_vector, mask, VECTOR_WIDTH);
                break;
            case ExecutionType::Store:
                criticalPathInstructions[child].storeInstructions.maskedAdd(
                    one_vector, mask, VECTOR_WIDTH);
                break;
            case ExecutionType::BranchCond:
            case ExecutionType::BranchUncond:
                criticalPathInstructions[child].branchInstructions.maskedAdd(
                    one_vector, mask, VECTOR_WIDTH);
                break;
            default:
                criticalPathInstructions[child].otherInstructions.maskedAdd(
                    one_vector, mask, VECTOR_WIDTH);
        }
    }

    if (parent.type == VertexType::InstrFetch)
    {
        if (child.type == VertexType::InstrFetch)
        {
            weight.smallerThanOrEqual(l2iThreshold, mask, comparison, VECTOR_WIDTH);
            criticalPathCycles[child].goodFetchHitCycles.maskedAdd(
                weight, comparison, VECTOR_WIDTH);
            
            weight.largerThan(l2iThreshold, mask, comparison, VECTOR_WIDTH);
            criticalPathCycles[child].goodFetchMissCycles.maskedAdd(
                weight, comparison, VECTOR_WIDTH);
        }
        else // child.type == VertexType::InstrDispatch
        {
            criticalPathCycles[child].decodeCycles.maskedAdd(
                weight, mask, VECTOR_WIDTH);
        }
    }
    else if (parent.type == VertexType::InstrDispatch)
    {
        criticalPathCycles[child].dispatchCycles.maskedAdd(
            weight, mask, VECTOR_WIDTH);
    }
    else if (parent.type == VertexType::InstrExecute)
    {
        if ((child.type == VertexType::InstrDispatch) ||
            (child.type == VertexType::InstrExecute)  ||
            (child.type == VertexType::MemExecute)    ||
            (child.type == VertexType::InstrCommit))
        {
            int parent_execution_type = executionType[parent.instrNum % AnalysisWindow];
            switch (parent_execution_type)
            {
                case ExecutionType::IntBase:
                case ExecutionType::IntMul:
                case ExecutionType::IntDiv:
                    criticalPathCycles[child].intCycles.maskedAdd(
                        weight, mask, VECTOR_WIDTH);
                    break;
                case ExecutionType::FpBase:
                case ExecutionType::FpMul:
                case ExecutionType::FpDiv:
                    criticalPathCycles[child].fpCycles.maskedAdd(
                        weight, mask, VECTOR_WIDTH);
                    break;
                case ExecutionType::Load:
                case ExecutionType::Store:
                    criticalPathCycles[child].lsCycles.maskedAdd(
                        weight, mask, VECTOR_WIDTH);
                    break;
                case ExecutionType::BranchCond:
                case ExecutionType::BranchUncond:
                    criticalPathCycles[child].branchCycles.maskedAdd(
                        weight, mask, VECTOR_WIDTH);
                    break;
                case ExecutionType::Syscall:
                    criticalPathCycles[child].syscallCycles.maskedAdd(
                        weight, mask, VECTOR_WIDTH);
                    break;
                case ExecutionType::Atomic:
                    criticalPathCycles[child].atomicCycles.maskedAdd(
                        weight, mask, VECTOR_WIDTH);
                    break;
                default: // ExecutionType::Other
                    criticalPathCycles[child].otherCycles.maskedAdd(
                        weight, mask, VECTOR_WIDTH);
            }
        }
        else // child.type == VertexType::InstrFetch
        {
            // The weight equals total cycles of RscIntAlu + mispredictionPenalty + fetchCycles

            Vector br_weight(intAluTotalCycles);
            criticalPathCycles[child].branchCycles.maskedAdd(
                br_weight, mask, VECTOR_WIDTH);

            Vector fetch_weight(weight, intAluTotalCycles);

            fetch_weight.smallerThanOrEqual(l2iThreshold, mask, comparison, VECTOR_WIDTH);
            criticalPathCycles[child].badFetchHitCycles.maskedAdd(
                fetch_weight, comparison, VECTOR_WIDTH);

            fetch_weight.largerThan(l2iThreshold, mask, comparison, VECTOR_WIDTH);
            criticalPathCycles[child].badFetchMissCycles.maskedAdd(
                fetch_weight, comparison, VECTOR_WIDTH);
        }
    }
    else if (parent.type == VertexType::MemExecute)
    {
        int parent_execution_type = executionType[parent.instrNum % AnalysisWindow];
        if (parent_execution_type == ExecutionType::Load)
        {
            weight.smallerThanOrEqual(l1dThreshold, mask, comparison, VECTOR_WIDTH);
            criticalPathCycles[child].loadL1HitCycles.maskedAdd(
                weight, comparison, VECTOR_WIDTH);

            weight.between(l1dThreshold, l2dThreshold, mask, comparison, VECTOR_WIDTH);
            criticalPathCycles[child].loadL2HitCycles.maskedAdd(
                weight, comparison, VECTOR_WIDTH);

            weight.largerThan(l2dThreshold, mask, comparison, VECTOR_WIDTH);
            criticalPathCycles[child].loadMissCycles.maskedAdd(
                weight, comparison, VECTOR_WIDTH);
        }
        else // parentExecutionType == ExecutionType::Store
        {
            weight.smallerThanOrEqual(l1dThreshold, mask, comparison, VECTOR_WIDTH);
            criticalPathCycles[child].storeL1HitCycles.maskedAdd(
                weight, comparison, VECTOR_WIDTH);

            weight.between(l1dThreshold, l2dThreshold, mask, comparison, VECTOR_WIDTH);
            criticalPathCycles[child].storeL2HitCycles.maskedAdd(
                weight, comparison, VECTOR_WIDTH);

            weight.largerThan(l2dThreshold, mask, comparison, VECTOR_WIDTH);
            criticalPathCycles[child].storeMissCycles.maskedAdd(
                weight, comparison, VECTOR_WIDTH);
        }
    }
    else // parent.type == VertexType::InstrCommit
    {
        criticalPathCycles[child].commitCycles.maskedAdd(
            weight, mask, VECTOR_WIDTH);
    }
    // cout << parent.instrNum << '\t' << parent.type << '\t' << child.instrNum << '\t' << child.type << '\t' << \
    //     criticalPathCycles[child].goodFetchHitCycles.vec[0] + \
    //     criticalPathCycles[child].goodFetchMissCycles.vec[0] + \
    //     criticalPathCycles[child].badFetchHitCycles.vec[0] + \
    //     criticalPathCycles[child].badFetchMissCycles.vec[0] + \
    //     criticalPathCycles[child].decodeCycles.vec[0] + \
    //     criticalPathCycles[child].dispatchCycles.vec[0] + \
    //     criticalPathCycles[child].intCycles.vec[0] + \
    //     criticalPathCycles[child].fpCycles.vec[0] + \
    //     criticalPathCycles[child].lsCycles.vec[0] + \
    //     criticalPathCycles[child].loadL1HitCycles.vec[0] + \
    //     criticalPathCycles[child].loadL2HitCycles.vec[0] + \
    //     criticalPathCycles[child].loadMissCycles.vec[0] + \
    //     criticalPathCycles[child].storeL1HitCycles.vec[0] + \
    //     criticalPathCycles[child].storeL2HitCycles.vec[0] + \
    //     criticalPathCycles[child].storeMissCycles.vec[0] + \
    //     criticalPathCycles[child].branchCycles.vec[0] + \
    //     criticalPathCycles[child].syscallCycles.vec[0] + \
    //     criticalPathCycles[child].atomicCycles.vec[0] + \
    //     criticalPathCycles[child].otherCycles.vec[0] + \
    //     criticalPathCycles[child].commitCycles.vec[0]
    //      << endl;
}

void Graph::recordStats(bool show_details, bool hopping_window)
{
    fstream result_file;
    result_file.open(resultFileName, fstream::out | fstream::app);
    result_file << "==============================================================" << endl;
    result_file << traceFileName << endl;

    ostringstream os;

    uint64_t window_instructions = instrCount - analyzedWindows * AnalysisWindow;

    os << setprecision(4);

    Vertex last_vertex(VertexType::InstrCommit, instrCount - 1);
    for (uint32_t i = 0; i < VECTOR_WIDTH; ++i)
    {
        os << "--------------------------------------------------------------" << endl;
        os << "*** ";
        if (hopping_window)
        {
            os << "Window " << analyzedWindows << ", ";
        }
        os << "Scenario " << (int)i << endl << endl;

        os << "Total instructions count:  " << instrCount << endl;
        if (hopping_window)
        {
            os << "Window instructions count: " << window_instructions << endl << endl;
        }

        os << "Length: " << length[last_vertex][i] << endl;
        os << "ILP:    " << ((double)window_instructions / (double)length[last_vertex][i]) << endl;
        os << "CPI:    " << ((double)length[last_vertex][i] / (double)window_instructions) << endl;

        if (show_details)
        {
            os << endl;

            double good_fetch_hit_cycles =
                ((double)criticalPathCycles[last_vertex].goodFetchHitCycles[i] /
                 (double)length[last_vertex][i]) * 100;
            double good_fetch_miss_cycles =
                ((double)criticalPathCycles[last_vertex].goodFetchMissCycles[i] /
                 (double)length[last_vertex][i]) * 100;
            double bad_fetch_hit_cycles =
                ((double)criticalPathCycles[last_vertex].badFetchHitCycles[i] /
                 (double)length[last_vertex][i]) * 100;
            double bad_fetch_miss_cycles =
                ((double)criticalPathCycles[last_vertex].badFetchMissCycles[i] /
                 (double)length[last_vertex][i]) * 100;
            double decode_cycles =
                ((double)criticalPathCycles[last_vertex].decodeCycles[i] /
                 (double)length[last_vertex][i]) * 100;
            double dispatch_cycles =
                ((double)criticalPathCycles[last_vertex].dispatchCycles[i] /
                 (double)length[last_vertex][i]) * 100;
            double int_cycles =
                ((double)criticalPathCycles[last_vertex].intCycles[i] /
                 (double)length[last_vertex][i]) * 100;
            double fp_cycles =
                ((double)criticalPathCycles[last_vertex].fpCycles[i] /
                 (double)length[last_vertex][i]) * 100;
            double ls_cycles =
                ((double)criticalPathCycles[last_vertex].lsCycles[i] /
                 (double)length[last_vertex][i]) * 100;
            double load_l1_hit_cycles =
                ((double)criticalPathCycles[last_vertex].loadL1HitCycles[i] /
                 (double)length[last_vertex][i]) * 100;
            double load_l2_hit_cycles =
                ((double)criticalPathCycles[last_vertex].loadL2HitCycles[i] /
                 (double)length[last_vertex][i]) * 100;
            double load_miss_cycles =
                ((double)criticalPathCycles[last_vertex].loadMissCycles[i] /
                 (double)length[last_vertex][i]) * 100;
            double store_l1_hit_cycles =
                ((double)criticalPathCycles[last_vertex].storeL1HitCycles[i] /
                 (double)length[last_vertex][i]) * 100;
            double store_l2_hit_cycles =
                ((double)criticalPathCycles[last_vertex].storeL2HitCycles[i] /
                 (double)length[last_vertex][i]) * 100;
            double store_miss_cycles =
                ((double)criticalPathCycles[last_vertex].storeMissCycles[i] /
                 (double)length[last_vertex][i]) * 100;
            double branch_cycles =
                ((double)criticalPathCycles[last_vertex].branchCycles[i] /
                 (double)length[last_vertex][i]) * 100;
            double syscall_cycles =
                ((double)criticalPathCycles[last_vertex].syscallCycles[i] /
                 (double)length[last_vertex][i]) * 100;
            double atomic_cycles =
                ((double)criticalPathCycles[last_vertex].atomicCycles[i] /
                 (double)length[last_vertex][i]) * 100;
            double other_cycles =
                ((double)criticalPathCycles[last_vertex].otherCycles[i] /
                 (double)length[last_vertex][i]) * 100;
            double commit_cycles =
                ((double)criticalPathCycles[last_vertex].commitCycles[i] /
                 (double)length[last_vertex][i]) * 100;

            uint64_t critical_instructions =
                criticalPathInstructions[last_vertex].intInstructions[i] +
                criticalPathInstructions[last_vertex].fpInstructions[i] +
                criticalPathInstructions[last_vertex].loadInstructions[i] +
                criticalPathInstructions[last_vertex].storeInstructions[i] +
                criticalPathInstructions[last_vertex].branchInstructions[i] +
                criticalPathInstructions[last_vertex].otherInstructions[i];
            double int_instructions_critical =
                ((double)criticalPathInstructions[last_vertex].intInstructions[i] /
                 (double)critical_instructions) * 100;
            double fp_instructions_critical =
                ((double)criticalPathInstructions[last_vertex].fpInstructions[i] /
                 (double)critical_instructions) * 100;
            double load_instructions_critical =
                ((double)criticalPathInstructions[last_vertex].loadInstructions[i] /
                 (double)critical_instructions) * 100;
            double store_instructions_critical =
                ((double)criticalPathInstructions[last_vertex].storeInstructions[i] /
                 (double)critical_instructions) * 100;
            double branch_instructions_critical =
                ((double)criticalPathInstructions[last_vertex].branchInstructions[i] /
                 (double)critical_instructions) * 100;
            double other_instructions_critical =
                ((double)criticalPathInstructions[last_vertex].otherInstructions[i] /
                 (double)critical_instructions) * 100;

            double int_instructions_all =
                ((double)instructionMix[0] /
                 (double)window_instructions) * 100;
            double fp_instructions_all =
                ((double)instructionMix[1] /
                 (double)window_instructions) * 100;
            double load_instructions_all =
                ((double)instructionMix[2] /
                 (double)window_instructions) * 100;
            double store_instructions_all =
                ((double)instructionMix[3] /
                 (double)window_instructions) * 100;
            double branch_instructions_all =
                ((double)instructionMix[4] /
                 (double)window_instructions) * 100;
            double other_instructions_all =
                ((double)instructionMix[5] /
                 (double)window_instructions) * 100;

            os << "Good fetch hit cycles:  "
               << good_fetch_hit_cycles << "% ("
               << criticalPathCycles[last_vertex].goodFetchHitCycles[i] << ")" << endl;
            os << "Good fetch miss cycles: "
               << good_fetch_miss_cycles << "% ("
               << criticalPathCycles[last_vertex].goodFetchMissCycles[i] << ")" << endl;
            os << "Bad fetch hit cycles:   "
               << bad_fetch_hit_cycles << "% ("
               << criticalPathCycles[last_vertex].badFetchHitCycles[i] << ")" << endl;
            os << "Bad fetch miss cycles:  "
               << bad_fetch_miss_cycles << "% ("
               << criticalPathCycles[last_vertex].badFetchMissCycles[i] << ")" << endl;
            os << "Decode cycles:          "
               << decode_cycles << "% ("
               << criticalPathCycles[last_vertex].decodeCycles[i] << ")" << endl;
            os << "Dispatch cycles:        "
               << dispatch_cycles << "% ("
               << criticalPathCycles[last_vertex].dispatchCycles[i] << ")" << endl;
            os << "Int cycles:             "
               << int_cycles << "% ("
               << criticalPathCycles[last_vertex].intCycles[i] << ")" << endl;
            os << "FP cycles:              "
               << fp_cycles << "% ("
               << criticalPathCycles[last_vertex].fpCycles[i] << ")" << endl;
            os << "LS cycles:              "
               << ls_cycles << "% ("
               << criticalPathCycles[last_vertex].lsCycles[i] << ")" << endl;
            os << "Load L1 hit cycles:     "
               << load_l1_hit_cycles << "% ("
               << criticalPathCycles[last_vertex].loadL1HitCycles[i] << ")" << endl;
            os << "Load L2 hit cycles:     "
               << load_l2_hit_cycles << "% ("
               << criticalPathCycles[last_vertex].loadL2HitCycles[i] << ")" << endl;
            os << "Load miss cycles:       "
               << load_miss_cycles << "% ("
               << criticalPathCycles[last_vertex].loadMissCycles[i] << ")" << endl;
            os << "Store L1 hit cycles:    "
               << store_l1_hit_cycles << "% ("
               << criticalPathCycles[last_vertex].storeL1HitCycles[i] << ")" << endl;
            os << "Store L2 hit cycles:    "
               << store_l2_hit_cycles << "% ("
               << criticalPathCycles[last_vertex].storeL2HitCycles[i] << ")" << endl;
            os << "Store miss cycles:      "
               << store_miss_cycles << "% ("
               << criticalPathCycles[last_vertex].storeMissCycles[i] << ")" << endl;
            os << "Branch cycles:          "
               << branch_cycles << "% ("
               << criticalPathCycles[last_vertex].branchCycles[i] << ")" << endl;
            os << "Syscall cycles:         "
               << syscall_cycles << "% ("
               << criticalPathCycles[last_vertex].syscallCycles[i] << ")" << endl;
            os << "Atomic cycles:          "
               << atomic_cycles << "% ("
               << criticalPathCycles[last_vertex].atomicCycles[i] << ")" << endl;
            os << "Other cycles:           "
               << other_cycles << "% ("
               << criticalPathCycles[last_vertex].otherCycles[i] << ")" << endl;
            os << "Commit cycles:          "
               << commit_cycles << "% ("
               << criticalPathCycles[last_vertex].commitCycles[i] << ")" << endl;

            os << endl;

            os << "Critical int instructions:    "
               << int_instructions_critical << "% ("
               << criticalPathInstructions[last_vertex].intInstructions[i] << ")" << endl;
            os << "Critical fp instructions:     "
               << fp_instructions_critical << "% ("
               << criticalPathInstructions[last_vertex].fpInstructions[i] << ")" << endl;
            os << "Critical load instructions:   "
               << load_instructions_critical << "% ("
               << criticalPathInstructions[last_vertex].loadInstructions[i] << ")" << endl;
            os << "Critical store instructions:  "
               << store_instructions_critical << "% ("
               << criticalPathInstructions[last_vertex].storeInstructions[i] << ")" << endl;
            os << "Critical branch instructions: "
               << branch_instructions_critical << "% ("
               << criticalPathInstructions[last_vertex].branchInstructions[i] << ")" << endl;
            os << "Critical other instructions:  "
               << other_instructions_critical << "% ("
               << criticalPathInstructions[last_vertex].otherInstructions[i] << ")" << endl;
            os << "All int instructions:         "
               << int_instructions_all << "% ("
               << instructionMix[0] << ")" << endl;
            os << "All fp instructions:          "
               << fp_instructions_all << "% ("
               << instructionMix[1] << ")" << endl;
            os << "All load instructions:        "
               << load_instructions_all << "% ("
               << instructionMix[2] << ")" << endl;
            os << "All store instructions:       "
               << store_instructions_all << "% ("
               << instructionMix[3] << ")" << endl;
            os << "All branch instructions:      "
               << branch_instructions_all << "% ("
               << instructionMix[4] << ")" << endl;
            os << "All other instructions:       "
               << other_instructions_all << "% ("
               << instructionMix[5] << ")" << endl;

            os << endl;

            os << "L1i MPKI:        "
               << ((double)(1000 * l1iMisses) / (double)window_instructions) << endl;
            os << "L2i MPKI:        "
               << ((double)(1000 * l2iMisses) / (double)window_instructions) << endl;
            os << "L1d MPKI:        "
               << ((double)(1000 * l1dMisses) / (double)window_instructions) << endl;
            os << "L2d MPKI:        "
               << ((double)(1000 * l2dMisses) / (double)window_instructions) << endl;
            os << "BP MPKI:         "
               << ((double)(1000 * bpMisses) / (double)window_instructions) << endl;
            os << "BP accuracy (%): "
               << ((double)(100 * (branchCount - bpMisses)) / (double)branchCount) << endl;

            os << endl;
        }
    }

    string str = os.str();
    cout << str;
    result_file << str;

    /*
    if (show_details)
    {
        cout << endl;
        if (icache != NULL)
        {
            cout << "***** I-cache stats *****" << endl;
            icache->printStats();
        }
        if (dcache != NULL)
        {
            cout << "***** D-cache stats *****" << endl;
            dcache->printStats();
        }
    }
    */

    cout << "--------------------------------------------------------------" << endl;
}

void Graph::printEdge(Vertex& parent, OutgoingEdge& e)
{
    cout << "*** Edge: "
         << parent.instrNum << "," << (int)parent.type << " to " 
         << e.child.instrNum << "," << (int)e.child.type << "; " 
         << e.weight.toString() << endl;
}

void Graph::printEdge(Vertex& child, IncomingEdge& e)
{
    cout << "*** Edge: "
         << e.parent.instrNum << "," << (int)e.parent.type << " to " 
         << child.instrNum << "," << (int)child.type << "; " 
         << e.weight.toString() << endl;
}
