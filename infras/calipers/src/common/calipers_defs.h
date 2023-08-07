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


#ifndef CALIPERS_DEFS_H
#define CALIPERS_DEFS_H

#include <iostream>
#include <chrono>

using namespace std;

#define CALIPERS_INFO(msg) \
    cerr << "[INFO]: " << msg << endl;

#define CALIPERS_WARNING(warn_msg) \
    cerr << "[WARN]: " << warn_msg << endl;

#define CALIPERS_ERROR(error_msg) \
    {cerr << "[ERROR]: " << error_msg << endl; exit(-1);}

#define RAND_SEED 27302730

#define TICKS_PER_CYCLE 1 // 1000 // Access times in the trace are given in ticks.
 
#define CACHE_LINE_BYTES     64
#define CACHE_ADDRESS_ZEROS  6 

#define MAX_REG_RD   3 // Maximum number of registers read
#define MAX_REG_WR   1 // Maximum number of registers written
#define MAX_OPERANDS (MAX_REG_RD + MAX_REG_WR)

#define INO_WINDOW  400
#define MAX_PARENTS 10

#define OOO_HOPPING_WINDOW 10000000
#define OOO_SLIDING_WINDOW 800

#define VECTOR_WIDTH 1

template <class Duration>
using sys_time = chrono::time_point<chrono::system_clock, Duration>;
using sys_nanoseconds = sys_time<chrono::nanoseconds>;


#endif // CALIPERS_DEFS_H
