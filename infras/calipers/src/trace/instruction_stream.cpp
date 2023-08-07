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
#include <fstream>
#include <string>
#include <cstdlib>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <vector>

#include "calipers_defs.h"
#include "instruction_stream.h"
#include "calipers_util.h"

using namespace std;


InstructionStream::InstructionStream(
    string m5_trace_file_name,
    bool trace_bp,
    bool trace_icache,
    bool trace_dcache
):
    traceBP(trace_bp),
    traceICache(trace_icache),
    traceDCache(trace_dcache) {

    m5_trace_file.open(m5_trace_file_name);
    if (!m5_trace_file.is_open()) {
        CALIPERS_ERROR("Unable to open the M5 trace file");
    }
    next_m5_trace_file.open(m5_trace_file_name);
	calipers_trace_file_name = "trace/" + get_benchmark_name(m5_trace_file_name) + '-' +  get_linux_base_name(m5_trace_file_name);
    convert_m5_trace_to_calipers_trace();
    // traceFile.open(m5_trace_file_name);
    traceFile.open(calipers_trace_file_name);
    if (!traceFile.is_open()) {
        CALIPERS_ERROR("Unable to open the trace file");
    }
}

void InstructionStream::convert_m5_trace_to_calipers_trace() {
    traceFile.open(calipers_trace_file_name, fstream::out);
    string line, type, inst, next_line;
    getline(next_m5_trace_file, next_line);
    long double last_fetch_cache_line = 0;
    while (getline(m5_trace_file, line)) {
        getline(next_m5_trace_file, next_line);
        vector<string> segments, temp;
        boost::split(segments, line, boost::is_any_of(":"), boost::token_compress_on);
        traceFile << "@I ";
        boost::split(temp, segments[3], boost::is_any_of(" "), boost::token_compress_on);
        inst = lstrip(rstrip(segments[4]));
		if (inst.find("fence") != string::npos) {
			traceFile << temp[1] << " fence";
		} else if (inst.find("amoswap_w") != string::npos) {
			inst.replace(0, 12, "amoswap_w");
			traceFile << temp[1] << " " << inst;
		} else if (inst.find("fmv_x_w") != string::npos) {
			inst.replace(0, string("fmv_x_w").length(), "fmv_x_s");
			traceFile << temp[1] << " " << inst;
		} else if (inst.find("fmv_w_x") != string::npos) {
			inst.replace(0, string("fmv_s_x").length(), "fmv_s_x");
			traceFile << temp[1] << " " << inst;
		} else {
        	traceFile << temp[1] << " " << inst;
		}
        type = rstrip(lstrip(segments[5]));
        if (type == "MemRead" || type == "MemWrite" || type == "FloatMemRead" || type == "FloatMemWrite") {
            boost::split(temp, segments[6], boost::is_any_of(" "), boost::token_compress_on);
            traceFile << " @A " << temp[2].substr(2);
        }
        traceFile << endl;
        // generate the fetch latency
        boost::split(temp, segments[7], boost::is_any_of("="), boost::token_compress_on);
        auto fetch_cache_line = stold(temp[1]) / 1000;
        if (last_fetch_cache_line == fetch_cache_line) {
            traceFile << "@F " << 0 << endl;
        }
        else {
            boost::split(temp, segments[8], boost::is_any_of("="), boost::token_compress_on);
            auto process_cache_completion = stold(temp[1]) / 1000;
            traceFile << "@F " << process_cache_completion - fetch_cache_line << endl;
        }
        last_fetch_cache_line = fetch_cache_line;
        // generate the branch prediction outcome
        // 0 if mispredicted; 1 if correctly predicted or the instruction is not a branch
        if (starts_with(inst, "beq") || starts_with(inst, "bne") || \
            starts_with(inst, "bltu") || starts_with(inst, "blt") || \
            starts_with(inst, "bgeu") || starts_with(inst, "bge") || \
            starts_with(inst, "jalr") || starts_with(inst, "jal")
        ) {
            vector<string> next_segments, next_temp;
            if (!next_m5_trace_file.eof()) {
                boost::split(next_segments, next_line, boost::is_any_of(":"), boost::token_compress_on);
                boost::split(next_temp, next_segments[7], boost::is_any_of("="), boost::token_compress_on);
                auto fetch_cache_line = stold(next_temp[1]) / 1000;
                boost::split(temp, segments[23], boost::is_any_of("="), boost::token_compress_on);
                auto complete = stold(temp[1]) / 1000;
                if (complete < fetch_cache_line)
                    traceFile << "@B 0" << endl;
                else
                    traceFile << "@B 1" << endl;
            } else {
                traceFile << "@B 1" << endl;
            }
        } else {
            traceFile << "@B 1" << endl;
        }
        // generate the memory access latency
        if (type == "MemRead" || type == "MemWrite" || type == "FloatMemRead" || type == "FloatMemWrite") {
            if (type == "MemRead") {
                boost::split(temp, segments[23], boost::is_any_of("="), boost::token_compress_on);
                auto complete = stold(temp[1]) / 1000;
                boost::split(temp, segments[24], boost::is_any_of("="), boost::token_compress_on);
                auto complete_memory = stold(temp[1]) / 1000;
                traceFile <<"@M " << complete_memory - complete << endl;
            } else {
                traceFile << "@M 0" << endl;
            }
        }
    }
    traceFile.close();
}
