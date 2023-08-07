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

#ifndef RISCV_STREAM_H
#define RISCV_STREAM_H

#include <unordered_map>
#include "instruction_stream.h"

/**
 * Defining how a RISC-V stream of instructions is parsed
 * Based on: "The RISC-V Instruction Set Manual" (Version 2.2)
 */
class RiscvStream : public InstructionStream
{
  private:
    enum IntReg
    {
        zero = 0,
        ra = 1,
        sp = 2,
        gp = 3,
        tp = 4,
        t0 = 5,
        t1 = 6,
        t2 = 7,
        s0 = 8,
        fp = 8,
        s1 = 9,
        a0 = 10,
        a1 = 11,
        a2 = 12,
        a3 = 13,
        a4 = 14,
        a5 = 15,
        a6 = 16,
        a7 = 17,
        s2 = 18,
        s3 = 19,
        s4 = 20,
        s5 = 21,
        s6 = 22,
        s7 = 23,
        s8 = 24,
        s9 = 25,
        s10 = 26,
        s11 = 27,
        t3 = 28,
        t4 = 29,
        t5 = 30,
        t6 = 31,
        pc = 32,
        last = 32
    }; // enum IntReg

    enum FpReg
    {
        ft0 = IntReg::last + 1,
        ft1,
        ft2,
        ft3,
        ft4,
        ft5,
        ft6,
        ft7,
        fs0,
        fs1,
        fa0,
        fa1,
        fa2,
        fa3,
        fa4,
        fa5,
        fa6,
        fa7,
        fs2,
        fs3,
        fs4,
        fs5,
        fs6,
        fs7,
        fs8,
        fs9,
        fs10,
        fs11,
        ft8,
        ft9,
        ft10,
        ft11
    }; // enum FpReg

    enum Csr
    {
        Ustatus = 0x000,
        Fflags = 0x001,
        Frm = 0x002,
        Fcsr = 0x003,
        Uie = 0x004,
        Utvec = 0x005,
        Uscratch = 0x040,
        Uepc = 0x041,
        Ucause = 0x042,
        Utval = 0x043,
        Uio = 0x044,
        Cycle = 0xc00,
        Time = 0xc01,
        Instret = 0xc02,
        Cycleh = 0xc80,
        Timeh = 0xc81,
        Instreth = 0xc82
    }; // enum Csr

    unordered_map<string, int> regMap;
    // Key: Register name, Value: Register number in the IntReg enum

    unordered_map<string, int> opcodeToTypeMap;
    // Key: Opcode, Value: ExecutionType

    unordered_map<string, char[MAX_OPERANDS]> syntaxMap;
    // Key: Opcode, Value: R/W characters for register read/write

    unordered_map<string, char> memAccessMap;
    // Key: Opcode, Value: L/S/A character for memory load/store/atomic operations

    unordered_map<string, uint32_t> memLengthMap;
    // Key: Opcode, Value: Memory access in bytes

    unordered_map<string, uint32_t> bytesMap;
    // Key: Opcode, Value: Number of instruction bytes

    string inst;
    string lastInstrLine;
    bool readFromFile = true;
    
    void initMaps();
    string parseNext(string& instr_line, size_t& current_pos);
    void parseInstr(string& instr_line);
    bool parseBranch(string& branch_line);
    uint32_t parseMemoryCycles(string& mem_line);
    uint32_t parseFetchCycles(string& fetch_line);
    string get_inst(string);

  public:
    RiscvStream(string trace_file_name, bool trace_bp, bool trace_icache, bool trace_dcache) :
                InstructionStream(trace_file_name, trace_bp, trace_icache, trace_dcache)
    {
        initMaps();
    }

    Instruction* next();
};

#endif // RISCV_STREAM_H
