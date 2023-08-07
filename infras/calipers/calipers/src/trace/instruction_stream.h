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

#ifndef INSTRUCTION_STREAM_H
#define INSTRUCTION_STREAM_H

#include <string>
#include <fstream>

#include "calipers_types.h"

using namespace std;

/**
 * The base class for reading a stream of instructions from a trace file
 * Dervied classes define how the trace is parsed based on the ISA specifications.
 */
class InstructionStream
{
  protected:
    fstream traceFile;
    ifstream m5_trace_file;
    string m5_trace_file_name;
    bool traceBP; // Whether the trace provides branch prdecition outcomes
    bool traceICache; // Whether the trace provides I-Cache access cycles
    bool traceDCache; // Whether the trace provides D-Cache access cycles
    Instruction instr;
    void convert_m5_trace_to_calipers_trace();

  public:
    InstructionStream(string m5_trace_file_name, bool trace_bp,
                      bool trace_icache, bool trace_dcache);
    virtual Instruction* next() = 0;
};

#endif // INSTRUCTION_STREAM_H
