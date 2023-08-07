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

#ifndef GRAPH_H
#define GRAPH_H

#include "calipers_types.h"
#include "instruction_stream.h"
#include "graph_util.h"
#include "cache.h"
#include "branch_predictor.h"


/**
 * The base class for graph-based modeling of a processor
 */
class Graph
{
  private:
    string traceFileName;
    string resultFileName;

  protected:
    struct VertexHash
    {
        uint64_t operator()(const Vertex& vertex) const
        {
            return (vertex.instrNum % AnalysisWindow) *
                   (VertexType::Last + 1) + vertex.type;
        }
    };

    struct VertexEqual
    {
       bool operator()(const Vertex& lhs, const Vertex& rhs) const
       {
          return (lhs.type == rhs.type) &&
                 (lhs.instrNum % AnalysisWindow == rhs.instrNum % AnalysisWindow);
       }
    };

    struct ScheduleComparison
    {
        bool operator()(const pair<uint64_t, int64_t>& lhs, const pair<uint64_t, int64_t>& rhs)
        {
            if (lhs.second != rhs.second)
            {
                return lhs.second < rhs.second;
            }
            else
            {
                return lhs.first < rhs.first;
            }
        }
    };

    struct VertexScheduleComparison
    {
        bool operator()(const pair<Vertex, int64_t>& lhs, const pair<Vertex, int64_t>& rhs)
        {
            if (lhs.second != rhs.second)
            {
                return lhs.second < rhs.second;
            }
            else if (lhs.first.instrNum != rhs.first.instrNum)
            {
                return lhs.first.instrNum < rhs.first.instrNum;
            }
            else
            {
                return lhs.first.type < rhs.first.type;
            }
        }
    };

    typedef std::set<pair<uint64_t, int64_t>, ScheduleComparison> ScheduleSet;
    // First: Instruction number, Second: Critical path length
    // Used for per-instruction (per-InstrExecute-vertex) scheduling

    typedef std::set<pair<Vertex, int64_t>, VertexScheduleComparison> VertexScheduleSet;
    // First: Vertex, Second: Critical path length
    // Used for per-vertex scheduling (for future use cases)

    InstructionStream* instrStream;

    Cache* icache;
    Cache* dcache;
    BranchPredictor* bp;

    // Parameters for calculating cache misses
    uint32_t l1iThreshold;
    uint32_t l2iThreshold;
    uint32_t l1dThreshold;
    uint32_t l2dThreshold;

    uint32_t intAluTotalCycles; // Ugly but OK

    unordered_map<uint64_t, int> executionType;
    // Key: Instruction number % AnalysisWindow, Value: ExecutionType (-1 for invalid)


    /*** Analysis outcome ***/

    // The size Vertex-key'ed maps are controlled through the corresponding key-equal function

    unordered_map<Vertex, Vector, VertexHash, VertexEqual> length;
    // length[v] = Length of the critical path to Vertex v
    //VertexToVectorMapExp lengthExp;

    unordered_map<Vertex, CycleTypes, VertexHash, VertexEqual> criticalPathCycles;
    // criticalPathCycles[v] = Composition of cycles on the critical path to Vertex v

    unordered_map<Vertex, InstructionTypes, VertexHash, VertexEqual> criticalPathInstructions;
    // criticalPathInstructions[v] = Composition of instructions on the critical path to Vertex v
    
    uint64_t instructionMix[6];
    // 0: int, 1: fp, 2: load, 3: store, 4: branch, 5: other

    // Execution time statistics
    uint64_t streamTime;
    uint64_t graphConstructionTime;
    uint64_t graphAnalysisTime;

    // Miscellaneous statistics
    uint64_t instrCount;
    uint64_t analyzedWindows;
    uint64_t l1iMisses;
    uint64_t l2iMisses;
    uint64_t l1dMisses;
    uint64_t l2dMisses;
    uint64_t bpMisses;
    uint64_t branchCount;


    void updateCriticalPathCycles(Vertex& parent, OutgoingEdge& e);
    void recordStats(bool show_details, bool hopping_window);
    void printEdge(Vertex& parent, OutgoingEdge& e);
    void printEdge(Vertex& child, IncomingEdge& e);

  public:
    static uint32_t AnalysisWindow;
    Graph(string trace_file_name, string result_file_name, InstructionStream* instr_stream);
    Graph() {}
    virtual void run() = 0;
};

#endif // GRAPH_H
