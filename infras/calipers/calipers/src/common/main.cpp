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

#include <stdint.h>
#include <string>
#include <fstream>
#include <sstream>
#include <unordered_map>

#include "calipers_defs.h"
#include "calipers_types.h"
#include "instruction_stream.h"
#include "riscv_stream.h"
#include "graph.h"
#include "inorder_core_graph.h"
#include "o3_core_graph.h"
#include "o3_core_graph_advanced.h"

using namespace std;

uint32_t Graph::AnalysisWindow;

void extract_config(string config_file_name, unordered_map<string, string>& config)
{
    ifstream config_file;
    config_file.open(config_file_name);
    if (!config_file.is_open())
    {
        CALIPERS_ERROR("Unable to open the config file");
    }

    string line;
    while (getline(config_file, line))
    {
        istringstream iss(line);
        string param, val;
        iss >> param;
        iss >> val;
        config[param] = val;
    }

    if (config["ISA"].compare("RISC-V") != 0)
    {
        CALIPERS_ERROR("Unsupproted ISA: " << config["ISA"]);
    }

    if ((config["Core"].compare("InO") != 0) &&
        (config["Core"].compare("OoO") != 0))
    {
        CALIPERS_ERROR("Unsupproted core: " << config["Core"]);
    }

    if ((config["Branch_Predictor"].compare("TraceB") != 0) &&
        (config["Branch_Predictor"].compare("StatisticalB") != 0))
    {
        CALIPERS_ERROR("Unsupproted branch predictor: " << config["Branch_Predictor"]);
    }

    if ((config["I_Cache"].compare("TraceC") != 0) &&
        (config["I_Cache"].compare("IdealC") != 0) &&
        (config["I_Cache"].compare("StatisticalC") != 0) &&
        (config["I_Cache"].compare("RealC") != 0))
    {
        CALIPERS_ERROR("Unsupproted I-cache: " << config["I_Cache"]);
    }

    if ((config["D_Cache"].compare("TraceC") != 0) &&
        (config["D_Cache"].compare("IdealC") != 0) &&
        (config["D_Cache"].compare("StatisticalC") != 0) &&
        (config["D_Cache"].compare("RealC") != 0))
    {
        CALIPERS_ERROR("Unsupproted D-cache: " << config["D_Cache"]);
    }
}

bool use_bp_model(unordered_map<string, string>& config)
{
    return (config["Branch_Predictor"].compare("TraceB") != 0);
}

bool use_icache_model(unordered_map<string, string>& config)
{
    return (config["I_Cache"].compare("TraceC") != 0);
}

bool use_dcache_model(unordered_map<string, string>& config)
{
    return (config["D_Cache"].compare("TraceC") != 0);
}

int bp_type(string str)
{
    int type;
    if (str.compare("TraceB") == 0)
    {
        type = BranchPredictorType::TraceB;
    }
    else // if (str.compare("StatisticalB") == 0)
    {
        type = BranchPredictorType::StatisticalB;
    }
    return type;
}

int cache_type(string str)
{
    int type;
    if (str.compare("TraceC") == 0)
    {
        type = CacheType::TraceC;
    }
    else if (str.compare("IdealC") == 0)
    {
        type = CacheType::IdealC;
    }
    else if (str.compare("StatisticalC") == 0)
    {
        type = CacheType::StatisticalC;
    }
    else // if (str.compare("RealC") == 0)
    {
        type = CacheType::RealC;
    }
    return type;
}

Graph* init(char* argv[], InstructionStream* instr_stream)
{
    srand(RAND_SEED); // For the statistical cache or branch preditor model, if used

    Graph* graph;
    unordered_map<string, string> config;
    extract_config(argv[1], config);

    bool trace_bp = !use_bp_model(config);
    bool trace_icache = !use_icache_model(config);
    bool trace_dcache = !use_dcache_model(config);

    instr_stream = new RiscvStream(
        argv[2], // Trace file name
        trace_bp,
        trace_icache,
        trace_dcache
    );

    if (config["Core"].compare("InO") == 0)
    {
        if (!(trace_bp && trace_icache && trace_dcache))
        {
            CALIPERS_ERROR(
                "Current InO model needs trace-provided branch prediction and load/store info");
        }

        Graph::AnalysisWindow = INO_WINDOW;
        graph = new InorderCoreGraph(argv[2], // Trace file name
                                     argv[3], // Result file name
                                     instr_stream,
                                     stoi(config["Fetch_Bandwidth"]),
                                     stoi(config["Dispatch_Bandwidth"]),
                                     stoi(config["Issue_Bandwidth"]),
                                     stoi(config["Commit_Bandwidth"]),
                                     stoi(config["Decode_Cycles"]),
                                     stoi(config["Dispatch_Cycles"]),
                                     stoi(config["Execute_To_Commit_Cycles"]),
                                     stoi(config["Prediction_Cycles"]),
                                     stoi(config["Misprediction_Penalty"]),
                                     stoi(config["Mem_Issue_Bandwidth"]),
                                     stoi(config["Mem_Commit_Bandwidth"]),
                                     stoi(config["Max_Mem_Accesses"]),
                                     stoi(config["Int_ALU_Count"]),
                                     stoi(config["Int_Mul_Count"]),
                                     stoi(config["Int_Div_Count"]),
                                     stoi(config["FPU_Count"]),
                                     stoi(config["LSU_Count"]),
                                     stoi(config["Load_Dependent_Early_Issue"]),
                                     stoi(config["Load_Early_Issue"]));

    }
    else //if (config["Core"].compare("OoO") == 0)
    {
        Graph::AnalysisWindow = OOO_HOPPING_WINDOW;
        graph = new O3CoreGraph(argv[2], // Trace file name
                                argv[3], // Result file name
                                instr_stream,
                                stoi(config["Instr_Buffer_Size"]),
                                stoi(config["Instr_Queue_Size"]),
                                stoi(config["Fetch_Bandwidth"]),
                                stoi(config["Dispatch_Bandwidth"]),
                                stoi(config["Issue_Bandwidth"]),
                                stoi(config["Commit_Bandwidth"]),
                                stoi(config["Decode_Cycles"]),
                                stoi(config["Dispatch_Cycles"]),
                                stoi(config["Execute_To_Commit_Cycles"]),
                                stoi(config["Prediction_Cycles"]),
                                stoi(config["Misprediction_Penalty"]),
                                stoi(config["Mem_Issue_Bandwidth"]),
                                stoi(config["Mem_Commit_Bandwidth"]),
                                stoi(config["Int_ALU_Count"]),
                                stoi(config["Int_Mul_Div_Count"]),
                                stoi(config["FP_ALU_Count"]),
                                stoi(config["FP_Mul_Div_Count"]),
                                stoi(config["LSU_Count"]),
                                stoi(config["LQ_Size"]),
                                stoi(config["SQ_Size"]),
                                bp_type(config["Branch_Predictor"]),
                                config["Branch_Predictor_Config"],
                                cache_type(config["I_Cache"]),
                                config["I_Cache_Config"],
                                cache_type(config["D_Cache"]),
                                config["D_Cache_Config"]);
        /*
        Graph::AnalysisWindow = OOO_SLIDING_WINDOW;
        graph = new O3CoreGraphAdvanced(argv[2], // Trace file name
                                        argv[3], // Result file name
                                        instr_stream,
                                        stoi(config["Instr_Buffer_Size"]),
                                        stoi(config["Instr_Queue_Size"]),
                                        stoi(config["Fetch_Bandwidth"]),
                                        stoi(config["Dispatch_Bandwidth"]),
                                        stoi(config["Issue_Bandwidth"]),
                                        stoi(config["Commit_Bandwidth"]),
                                        stoi(config["Decode_Cycles"]),
                                        stoi(config["Dispatch_Cycles"]),
                                        stoi(config["Execute_To_Commit_Cycles"]),
                                        stoi(config["Prediction_Cycles"]),
                                        stoi(config["Misprediction_Penalty"]),
                                        stoi(config["Mem_Issue_Bandwidth"]),
                                        stoi(config["Mem_Commit_Bandwidth"]),
                                        stoi(config["Int_ALU_Count"]),
                                        stoi(config["Int_Mul_Div_Count"]),
                                        stoi(config["FP_ALU_Count"]),
                                        stoi(config["FP_Mul_Div_Count"]),
                                        stoi(config["LSU_Count"]),
                                        stoi(config["LQ_Size"]),
                                        stoi(config["SQ_Size"]),
                                        bp_type(config["Branch_Predictor"]),
                                        config["Branch_Predictor_Config"],
                                        cache_type(config["I_Cache"]),
                                        config["I_Cache_Config"],
                                        cache_type(config["D_Cache"]),
                                        config["D_Cache_Config"]);
        */
    }

    return graph;
}

void finish(InstructionStream* instr_stream, Graph* graph)
{
    delete instr_stream;
    delete graph;
}


int main(int argc, char* argv[])
{
    if (argc != 4)
    {
        CALIPERS_ERROR("Usage --> arg1: config file, arg2: m5out trace file, arg3: result file");
    }

    InstructionStream* instr_stream;
    Graph* graph;

    graph = init(argv, instr_stream);
    graph->run();
    finish(instr_stream, graph);

    return 0;
}
