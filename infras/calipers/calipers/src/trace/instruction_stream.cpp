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

InstructionStream::InstructionStream(string m5_trace_file_name, bool trace_bp,
                                     bool trace_icache, bool trace_dcache) :
                                     m5_trace_file_name(m5_trace_file_name),
                                     traceBP(trace_bp),
                                     traceICache(trace_icache),
                                     traceDCache(trace_dcache)
{

    m5_trace_file.open(m5_trace_file_name);
    if (!m5_trace_file.is_open()) {
        CALIPERS_ERROR("Unable to open the M5 trace file");
    }
    convert_m5_trace_to_calipers_trace();
    traceFile.open(get_base_name(m5_trace_file_name));
    if (!traceFile.is_open()) {
        CALIPERS_ERROR("Unable to open the trace file");
    }
}


void InstructionStream::convert_m5_trace_to_calipers_trace() {
    traceFile.open(get_base_name(m5_trace_file_name), fstream::out);
    string line, type;
    while (getline(m5_trace_file, line)) {
        vector<string> segments, temp;
        boost::split(segments, line, boost::is_any_of(":"), boost::token_compress_on);
        traceFile << "@I ";
        boost::split(temp, segments[3], boost::is_any_of(" "), boost::token_compress_on);
        traceFile << temp[1] << " " << lstrip(rstrip(segments[4]));
        type = rstrip(lstrip(segments[5]));
        if (type == "MemRead" || type == "MemWrite") {
            boost::split(temp, segments[6], boost::is_any_of(" "), boost::token_compress_on);
            traceFile << " @A " << temp[2].substr(2);
        }
        traceFile << endl;
        // generate the fetch latency
        boost::split(temp, segments[7], boost::is_any_of("="), boost::token_compress_on);
        traceFile << "@F " << temp[1] << endl;
        // generate the branch prediction outcome
        boost::split(temp, segments[8], boost::is_any_of("="), boost::token_compress_on);
        traceFile << "@B " << 1 - stoi(temp[1]) << endl;
        // generate the memory access latency
        if (type == "MemRead" || type == "MemWrite") {
            boost::split(temp, segments[9], boost::is_any_of("="), boost::token_compress_on);
            traceFile << "@M " << temp[1] << endl;
        }
    }
    traceFile.close();
}
