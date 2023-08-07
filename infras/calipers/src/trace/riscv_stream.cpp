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
#include <string.h>

#include "calipers_defs.h"
#include "calipers_types.h"
#include "calipers_util.h"
#include "instruction_stream.h"
#include "riscv_stream.h"

using namespace std;


string RiscvStream::get_inst(string line) {
    // save the instruction for visualization
    auto temp = line.substr(line.find(' ', 3) + 1);
    temp = temp.substr(0, temp.find('@'));
    return lstrip(rstrip(temp));
}


Instruction* RiscvStream::next() {
    string line;

    while (true) {
        if (readFromFile) {
            if (!getline(traceFile, line)) {
                return NULL;
            }
        } else {
            line = lastInstrLine;
            readFromFile = true;
            if (line == "") {
                return NULL;
            }
        }

        //cout << "-----------" << endl;
        //cout << line << endl;

        if (line.find("@I ") == 0) {
            instr.inst = get_inst(line);

            parseInstr(line);
            lastInstrLine = line;

            if (traceICache) {
                string fetch_line;
                getline(traceFile, fetch_line);
                if (fetch_line.find("@F ") != 0) {
                    CALIPERS_ERROR(
                        "Expecting fetch cycles for \"" << line << \
                           "\" but getting \"" << fetch_line << "\""
                    );
                }
                instr.fetchCycles = parseFetchCycles(fetch_line);
            }

            if (traceBP) {
                string branch_line;
                getline(traceFile, branch_line);
                if (branch_line.find("@B ") != 0) {
                    CALIPERS_ERROR(
                        "Expecting branch prediction result for \"" << line << \
                            "\" but getting \"" << branch_line << "\""
                    );
                }
                instr.mispredicted = parseBranch(branch_line);

                if (instr.mispredicted &&
                    (instr.executionType != ExecutionType::BranchCond) && 
                    (instr.executionType != ExecutionType::BranchUncond) &&
                    (instr.executionType != ExecutionType::Syscall)) {
                    CALIPERS_WARNING("Misprediction for a regular instruction \"" << line << "\"");
                }
            }

            if (traceDCache) {
                if ((instr.executionType == ExecutionType::Load) ||
                    (instr.executionType == ExecutionType::Store) ||
                    (instr.executionType == ExecutionType::Atomic)) {
                    string mem_line;
                    getline(traceFile, mem_line);
                    if (mem_line.find("@M ") != 0) {
                        CALIPERS_WARNING("Expecting memory access cycles for \"" << line << "\"");
                        instr.lsCycles = 1;
                        lastInstrLine = mem_line;
                        readFromFile = false;
                    } else {
                        instr.lsCycles = parseMemoryCycles(mem_line);
                    }
                }
            }

            break;
        } else if ((line.find("@F") == 0) || (line.find("@B") == 0) || (line.find("@M") == 0)) {
            // Probably because of atomic instructions
            CALIPERS_WARNING("Ignoring \"" << line << "\" after \"" << lastInstrLine << "\"");
        } else {
            CALIPERS_ERROR("Invalid trace line \"" << line << "\"");
        }
    }
    return &instr;
}


string RiscvStream::parseNext(string& instr_line, size_t& current_pos) {
    size_t pos;

    if (current_pos == string::npos)
        return "";

    pos = instr_line.find(" ", current_pos);

    string str;
    
    if (pos == string::npos) {
        str = instr_line.substr(current_pos);
        current_pos = string::npos;
    } else {
        str = instr_line.substr(current_pos, pos - current_pos);
        current_pos = pos + 1;
    }


    pos = str.find("(");
    if (pos != string::npos) {
        // (operand) -> extract operand only
        size_t next_pos = str.find(")");
        return str.substr(pos + 1, next_pos - pos - 1);
    } else if (str[str.length() - 1] == ',') {
        // operand wo. comma
        return str.substr(0, str.length() - 1);
    } else {
        // pc, opcode, operand, @A
        return str;
    }
}

void RiscvStream::parseInstr(string& instr_line) {
    string opcode;
    string pc;
    string operands[MAX_OPERANDS];
    string mem_address;

    uint32_t operand_count = 0;
    bool mem_accessed = false;

    size_t current_pos = 3;

    pc = parseNext(instr_line, current_pos);

    opcode = parseNext(instr_line, current_pos);

    while (operand_count < MAX_OPERANDS) {
        string operand = parseNext(instr_line, current_pos);

        if (operand == "")
            break;

        if ((operand[0] >= 'a') && (operand[0] <= 'z')) {
            operands[operand_count] = operand;
            ++operand_count;
        }
        if (operand[0] == '@') {
            mem_accessed = true;
            break;
        }
    }

    if (mem_accessed) {
        mem_address = parseNext(instr_line, current_pos);
    }

    if (opcodeToTypeMap.count(opcode) == 0) {
        CALIPERS_ERROR("Invalid opcode \"" << instr_line << "\"");
    }

    instr.pc = stoull(pc, NULL, 16);
    instr.bytes = bytesMap[opcode];

    int executionType = opcodeToTypeMap[opcode];
    instr.executionType = executionType;

    if (executionType == ExecutionType::Atomic) {
        mem_address = "0xffffffffffffffff";
    }

    char* syntax = syntaxMap[opcode];
    uint32_t reg_read_count = 0;
    uint32_t reg_write_count = 0;

    for (uint32_t i = 0; i < operand_count; ++i) {
        int operand = regMap[operands[i]];

        if (operand == IntReg::zero)
            continue;

        if (syntax[i] == 'W') {
            instr.regWrite[reg_write_count] = operand;
            ++reg_write_count;
        } else if (syntax[i] == 'R') {
            instr.regRead[reg_read_count] = operand;
            ++reg_read_count;
        } else {
            CALIPERS_ERROR("Invalid operand \"" << instr_line << "\"");
        }
    }

    instr.regReadCount = reg_read_count;
    instr.regWriteCount = reg_write_count;

    if (mem_accessed) {
        char mem_access = memAccessMap[opcode];
        if (mem_access == 0) {
            CALIPERS_ERROR("Instruction should not access memory \"" << instr_line << "\"");
        }

        if (mem_access == 'L') {
            instr.memStoreCount = 0;
            instr.memLoadCount = 1;
            instr.memLoadBase = stoull(mem_address, NULL, 16);
            instr.memLoadLength = memLengthMap[opcode];
        } else if (mem_access == 'S') {
            instr.memLoadCount = 0;
            instr.memStoreCount = 1;
            instr.memStoreBase = stoull(mem_address, NULL, 16);
            instr.memStoreLength = memLengthMap[opcode];
        } else if (mem_access == 'A') {
            instr.memLoadCount = 1;
            instr.memStoreCount = 1;
            instr.memLoadBase = stoull(mem_address, NULL, 16);
            instr.memLoadLength = memLengthMap[opcode];
            instr.memStoreBase = instr.memLoadBase;
            instr.memStoreLength = instr.memLoadLength;
        } else {
            CALIPERS_ERROR("Invalid memory access \"" << instr_line << "\"");
        }
    } else {
        instr.memLoadCount = 0;
        instr.memStoreCount = 0;        
    }
}

bool RiscvStream::parseBranch(string& branch_line)
{
    //cout << branch_line << endl;

    size_t current_pos = 3;

    string prediction = parseNext(branch_line, current_pos);

    bool mispredicted;

    if (prediction[0] == '0') {
        mispredicted = true;
    } else if (prediction[0] == '1') {
        mispredicted = false;
    } else {
        CALIPERS_ERROR("Invalid branch prediction result");
    }

    return mispredicted;
}

uint32_t RiscvStream::parseMemoryCycles(string& mem_line)
{
    //cout << mem_line << endl;

    size_t current_pos = 3;

    string cycles = parseNext(mem_line, current_pos);

    uint32_t num = stoul(cycles) / TICKS_PER_CYCLE;

    return num;
}

uint32_t RiscvStream::parseFetchCycles(string& fetch_line)
{
    size_t current_pos = 3;

    string cycles = parseNext(fetch_line, current_pos);

    return stoul(cycles) / TICKS_PER_CYCLE;
}

void RiscvStream::initMaps() {
    // Initializing register maps

    regMap["zero"] = IntReg::zero;
    regMap["ra"] = IntReg::ra;
    regMap["sp"] = IntReg::sp;
    regMap["gp"] = IntReg::gp;
    regMap["tp"] = IntReg::tp;
    regMap["t0"] = IntReg::t0;
    regMap["t1"] = IntReg::t1;
    regMap["t2"] = IntReg::t2;
    regMap["s0"] = IntReg::s0;
    regMap["fp"] = IntReg::fp;
    regMap["s1"] = IntReg::s1;
    regMap["a0"] = IntReg::a0;
    regMap["a1"] = IntReg::a1;
    regMap["a2"] = IntReg::a2;
    regMap["a3"] = IntReg::a3;
    regMap["a4"] = IntReg::a4;
    regMap["a5"] = IntReg::a5;
    regMap["a6"] = IntReg::a6;
    regMap["a7"] = IntReg::a7;
    regMap["s2"] = IntReg::s2;
    regMap["s3"] = IntReg::s3;
    regMap["s4"] = IntReg::s4;
    regMap["s5"] = IntReg::s5;
    regMap["s6"] = IntReg::s6;
    regMap["s7"] = IntReg::s7;
    regMap["s8"] = IntReg::s8;
    regMap["s9"] = IntReg::s9;
    regMap["s10"] = IntReg::s10;
    regMap["s11"] = IntReg::s11;
    regMap["t3"] = IntReg::t3;
    regMap["t4"] = IntReg::t4;
    regMap["t5"] = IntReg::t5;
    regMap["t6"] = IntReg::t6;

    regMap["ft0"] = FpReg::ft0;
    regMap["ft1"] = FpReg::ft1;
    regMap["ft2"] = FpReg::ft2;
    regMap["ft3"] = FpReg::ft3;
    regMap["ft4"] = FpReg::ft4;
    regMap["ft5"] = FpReg::ft5;
    regMap["ft6"] = FpReg::ft6;
    regMap["ft7"] = FpReg::ft7;
    regMap["fs0"] = FpReg::fs0;
    regMap["fs1"] = FpReg::fs1;
    regMap["fa0"] = FpReg::fa0;
    regMap["fa1"] = FpReg::fa1;
    regMap["fa2"] = FpReg::fa2;
    regMap["fa3"] = FpReg::fa3;
    regMap["fa4"] = FpReg::fa4;
    regMap["fa5"] = FpReg::fa5;
    regMap["fa6"] = FpReg::fa6;
    regMap["fa7"] = FpReg::fa7;
    regMap["fs2"] = FpReg::fs2;
    regMap["fs3"] = FpReg::fs3;
    regMap["fs4"] = FpReg::fs4;
    regMap["fs5"] = FpReg::fs5;
    regMap["fs6"] = FpReg::fs6;
    regMap["fs7"] = FpReg::fs7;
    regMap["fs8"] = FpReg::fs8;
    regMap["fs9"] = FpReg::fs9;
    regMap["fs10"] = FpReg::fs10;
    regMap["fs11"] = FpReg::fs11;
    regMap["ft8"] = FpReg::ft8;
    regMap["ft9"] = FpReg::ft9;
    regMap["ft10"] = FpReg::ft10;
    regMap["ft11"] = FpReg::ft11;

	// chen.bai's' additions
	regMap["ustatus"] = Csr::Ustatus;
	regMap["fflags"] = Csr::Fflags;
	regMap["frm"] = Csr::Frm;
	regMap["fcsr"] = Csr::Fcsr;
	regMap["uie"] = Csr::Uie;
	regMap["utvec"] = Csr::Utvec;
	regMap["uscratch"] = Csr::Uscratch;
	regMap["uepc"] = Csr::Uepc;
	regMap["ucause"] = Csr::Ucause;
	regMap["utval"] = Csr::Utval;
	regMap["uio"] = Csr::Uio;
	regMap["cycle"] = Csr::Cycle;
	regMap["time"] = Csr::Time;
	regMap["instret"] = Csr::Instret;
	regMap["cycleh"] = Csr::Cycleh;
	regMap["timeh"] = Csr::Timeh;
	regMap["instreth"] = Csr::Instreth;


    // RV32I instructions

    opcodeToTypeMap["addi"] = ExecutionType::IntBase;
    strncpy(syntaxMap["addi"], "WR", MAX_OPERANDS);
    memAccessMap["addi"] = 0;
    memLengthMap["addi"] = 0;
    bytesMap["addi"] = 4;

    opcodeToTypeMap["slti"] = ExecutionType::IntBase;
    strncpy(syntaxMap["slti"], "WR", MAX_OPERANDS);
    memAccessMap["slti"] = 0;
    memLengthMap["slti"] = 0;
    bytesMap["slti"] = 4;

    opcodeToTypeMap["sltiu"] = ExecutionType::IntBase;
    strncpy(syntaxMap["sltiu"], "WR", MAX_OPERANDS);
    memAccessMap["sltiu"] = 0;
    memLengthMap["sltiu"] = 0;
    bytesMap["sltiu"] = 4;

    opcodeToTypeMap["andi"] = ExecutionType::IntBase;
    strncpy(syntaxMap["andi"], "WR", MAX_OPERANDS);
    memAccessMap["andi"] = 0;
    memLengthMap["andi"] = 0;
    bytesMap["andi"] = 4;

    opcodeToTypeMap["ori"] = ExecutionType::IntBase;
    strncpy(syntaxMap["ori"], "WR", MAX_OPERANDS);
    memAccessMap["ori"] = 0;
    memLengthMap["ori"] = 0;
    bytesMap["ori"] = 4;

    opcodeToTypeMap["xori"] = ExecutionType::IntBase;
    strncpy(syntaxMap["xori"], "WR", MAX_OPERANDS);
    memAccessMap["xori"] = 0;
    memLengthMap["xori"] = 0;
    bytesMap["xori"] = 4;

    opcodeToTypeMap["slli"] = ExecutionType::IntBase;
    strncpy(syntaxMap["slli"], "WR", MAX_OPERANDS);
    memAccessMap["slli"] = 0;
    memLengthMap["slli"] = 0;
    bytesMap["slli"] = 4;

    opcodeToTypeMap["srli"] = ExecutionType::IntBase;
    strncpy(syntaxMap["srli"], "WR", MAX_OPERANDS);
    memAccessMap["srli"] = 0;
    memLengthMap["srli"] = 0;
    bytesMap["srli"] = 4;

    opcodeToTypeMap["srai"] = ExecutionType::IntBase;
    strncpy(syntaxMap["srai"], "WR", MAX_OPERANDS);
    memAccessMap["srai"] = 0;
    memLengthMap["srai"] = 0;
    bytesMap["srai"] = 4;

    opcodeToTypeMap["lui"] = ExecutionType::IntBase;
    strncpy(syntaxMap["lui"], "W", MAX_OPERANDS);
    memAccessMap["lui"] = 0;
    memLengthMap["lui"] = 0;
    bytesMap["lui"] = 4;

    opcodeToTypeMap["auipc"] = ExecutionType::IntBase;
    strncpy(syntaxMap["auipc"], "W", MAX_OPERANDS);
    memAccessMap["auipc"] = 0;
    memLengthMap["auipc"] = 0;
    bytesMap["auipc"] = 4;

    opcodeToTypeMap["add"] = ExecutionType::IntBase;
    strncpy(syntaxMap["add"], "WRR", MAX_OPERANDS);
    memAccessMap["add"] = 0;
    memLengthMap["add"] = 0;
    bytesMap["add"] = 4;

    opcodeToTypeMap["slt"] = ExecutionType::IntBase;
    strncpy(syntaxMap["slt"], "WRR", MAX_OPERANDS);
    memAccessMap["slt"] = 0;
    memLengthMap["slt"] = 0;
    bytesMap["slt"] = 4;

    opcodeToTypeMap["sltu"] = ExecutionType::IntBase;
    strncpy(syntaxMap["sltu"], "WRR", MAX_OPERANDS);
    memAccessMap["sltu"] = 0;
    memLengthMap["sltu"] = 0;
    bytesMap["sltu"] = 4;

    opcodeToTypeMap["and"] = ExecutionType::IntBase;
    strncpy(syntaxMap["and"], "WRR", MAX_OPERANDS);
    memAccessMap["and"] = 0;
    memLengthMap["and"] = 0;
    bytesMap["and"] = 4;

    opcodeToTypeMap["or"] = ExecutionType::IntBase;
    strncpy(syntaxMap["or"], "WRR", MAX_OPERANDS);
    memAccessMap["or"] = 0;
    memLengthMap["or"] = 0;
    bytesMap["or"] = 4;

    opcodeToTypeMap["xor"] = ExecutionType::IntBase;
    strncpy(syntaxMap["xor"], "WRR", MAX_OPERANDS);
    memAccessMap["xor"] = 0;
    memLengthMap["xor"] = 0;
    bytesMap["xor"] = 4;

    opcodeToTypeMap["sll"] = ExecutionType::IntBase;
    strncpy(syntaxMap["sll"], "WRR", MAX_OPERANDS);
    memAccessMap["sll"] = 0;
    memLengthMap["sll"] = 0;
    bytesMap["sll"] = 4;

    opcodeToTypeMap["srl"] = ExecutionType::IntBase;
    strncpy(syntaxMap["srl"], "WRR", MAX_OPERANDS);
    memAccessMap["srl"] = 0;
    memLengthMap["srl"] = 0;
    bytesMap["srl"] = 4;

    opcodeToTypeMap["sub"] = ExecutionType::IntBase;
    strncpy(syntaxMap["sub"], "WRR", MAX_OPERANDS);
    memAccessMap["sub"] = 0;
    memLengthMap["sub"] = 0;
    bytesMap["sub"] = 4;

    opcodeToTypeMap["sra"] = ExecutionType::IntBase;
    strncpy(syntaxMap["sra"], "WRR", MAX_OPERANDS);
    memAccessMap["sra"] = 0;
    memLengthMap["sra"] = 0;
    bytesMap["sra"] = 4;

    opcodeToTypeMap["jal"] = ExecutionType::BranchUncond;
    strncpy(syntaxMap["jal"], "W", MAX_OPERANDS);
    memAccessMap["jal"] = 0;
    memLengthMap["jal"] = 0;
    bytesMap["jal"] = 4;

    opcodeToTypeMap["jalr"] = ExecutionType::BranchUncond;
    strncpy(syntaxMap["jalr"], "WR", MAX_OPERANDS);
    memAccessMap["jalr"] = 0;
    memLengthMap["jalr"] = 0;
    bytesMap["jalr"] = 4;

    opcodeToTypeMap["beq"] = ExecutionType::BranchCond;
    strncpy(syntaxMap["beq"], "RR", MAX_OPERANDS);
    memAccessMap["beq"] = 0;
    memLengthMap["beq"] = 0;
    bytesMap["beq"] = 4;

    opcodeToTypeMap["bne"] = ExecutionType::BranchCond;
    strncpy(syntaxMap["bne"], "RR", MAX_OPERANDS);
    memAccessMap["bne"] = 0;
    memLengthMap["bne"] = 0;
    bytesMap["bne"] = 4;

    opcodeToTypeMap["blt"] = ExecutionType::BranchCond;
    strncpy(syntaxMap["blt"], "RR", MAX_OPERANDS);
    memAccessMap["blt"] = 0;
    memLengthMap["blt"] = 0;
    bytesMap["blt"] = 4;

    opcodeToTypeMap["bltu"] = ExecutionType::BranchCond;
    strncpy(syntaxMap["bltu"], "RR", MAX_OPERANDS);
    memAccessMap["bltu"] = 0;
    memLengthMap["bltu"] = 0;
    bytesMap["bltu"] = 4;

    opcodeToTypeMap["bge"] = ExecutionType::BranchCond;
    strncpy(syntaxMap["bge"], "RR", MAX_OPERANDS);
    memAccessMap["bge"] = 0;
    memLengthMap["bge"] = 0;
    bytesMap["bge"] = 4;

    opcodeToTypeMap["bgeu"] = ExecutionType::BranchCond;
    strncpy(syntaxMap["bgeu"], "RR", MAX_OPERANDS);
    memAccessMap["bgeu"] = 0;
    memLengthMap["bgeu"] = 0;
    bytesMap["bgeu"] = 4;

    opcodeToTypeMap["lb"] = ExecutionType::Load;
    strncpy(syntaxMap["lb"], "WR", MAX_OPERANDS);
    memAccessMap["lb"] = 'L';
    memLengthMap["lb"] = 1;
    bytesMap["lb"] = 4;

    opcodeToTypeMap["lbu"] = ExecutionType::Load;
    strncpy(syntaxMap["lbu"], "WR", MAX_OPERANDS);
    memAccessMap["lbu"] = 'L';
    memLengthMap["lbu"] = 1;
    bytesMap["lbu"] = 4;

    opcodeToTypeMap["lh"] = ExecutionType::Load;
    strncpy(syntaxMap["lh"], "WR", MAX_OPERANDS);
    memAccessMap["lh"] = 'L';
    memLengthMap["lh"] = 2;
    bytesMap["lh"] = 4;

    opcodeToTypeMap["lhu"] = ExecutionType::Load;
    strncpy(syntaxMap["lhu"], "WR", MAX_OPERANDS);
    memAccessMap["lhu"] = 'L';
    memLengthMap["lhu"] = 2;
    bytesMap["lhu"] = 4;

    opcodeToTypeMap["lw"] = ExecutionType::Load;
    strncpy(syntaxMap["lw"], "WR", MAX_OPERANDS);
    memAccessMap["lw"] = 'L';
    memLengthMap["lw"] = 4;
    bytesMap["lw"] = 4;

    opcodeToTypeMap["sb"] = ExecutionType::Store;
    strncpy(syntaxMap["sb"], "RR", MAX_OPERANDS);
    memAccessMap["sb"] = 'S';
    memLengthMap["sb"] = 1;
    bytesMap["sb"] = 4;

    opcodeToTypeMap["sh"] = ExecutionType::Store;
    strncpy(syntaxMap["sh"], "RR", MAX_OPERANDS);
    memAccessMap["sh"] = 'S';
    memLengthMap["sh"] = 2;
    bytesMap["sh"] = 4;

    opcodeToTypeMap["sw"] = ExecutionType::Store;
    strncpy(syntaxMap["sw"], "RR", MAX_OPERANDS);
    memAccessMap["sw"] = 'S';
    memLengthMap["sw"] = 4;
    bytesMap["sw"] = 4;

    // NOTE: Be careful about the format of the disassembled instruction
    opcodeToTypeMap["fence"] = ExecutionType::Other;
    strncpy(syntaxMap["fence"], "WR", MAX_OPERANDS); // Both the source and destination registers
                                                     // must be the zero register
    memAccessMap["fence"] = 0;
    memLengthMap["fence"] = 0;
    bytesMap["fence"] = 4;

    // NOTE: Be careful about the format of the disassembled instruction
    opcodeToTypeMap["fence_i"] = ExecutionType::Other;
    strncpy(syntaxMap["fence_i"], "", MAX_OPERANDS);
    memAccessMap["fence_i"] = 0;
    memLengthMap["fence_i"] = 0;
    bytesMap["fence_i"] = 4;

    // NOTE: How is the CSR register shown in the disassembled instruction?
    opcodeToTypeMap["csrrw"] = ExecutionType::Other;
    strncpy(syntaxMap["csrrw"], "WRR", MAX_OPERANDS);
    memAccessMap["csrrw"] = 0;
    memLengthMap["csrrw"] = 0;
    bytesMap["csrrw"] = 4;

    // NOTE: How is the CSR register shown in the disassembled instruction?
    opcodeToTypeMap["csrrs"] = ExecutionType::Other;
    strncpy(syntaxMap["csrrs"], "WRR", MAX_OPERANDS);
    memAccessMap["csrrs"] = 0;
    memLengthMap["csrrs"] = 0;
    bytesMap["csrrs"] = 4;

    // NOTE: How is the CSR register shown in the disassembled instruction?
    opcodeToTypeMap["csrrc"] = ExecutionType::Other;
    strncpy(syntaxMap["csrrc"], "WRR", MAX_OPERANDS);
    memAccessMap["csrrc"] = 0;
    memLengthMap["csrrc"] = 0;
    bytesMap["csrrc"] = 4;

    // NOTE: How is the CSR register shown in the disassembled instruction?
    opcodeToTypeMap["csrrwi"] = ExecutionType::Other;
    strncpy(syntaxMap["csrrwi"], "WR", MAX_OPERANDS);
    memAccessMap["csrrwi"] = 0;
    memLengthMap["csrrwi"] = 0;
    bytesMap["csrrwi"] = 4;

    // NOTE: How is the CSR register shown in the disassembled instruction?
    opcodeToTypeMap["csrrsi"] = ExecutionType::Other;
    strncpy(syntaxMap["csrrsi"], "WR", MAX_OPERANDS);
    memAccessMap["csrrsi"] = 0;
    memLengthMap["csrrsi"] = 0;
    bytesMap["csrrsi"] = 4;

    // NOTE: How is the CSR register shown in the disassembled instruction?
    opcodeToTypeMap["csrrci"] = ExecutionType::Other;
    strncpy(syntaxMap["csrrci"], "WR", MAX_OPERANDS);
    memAccessMap["csrrci"] = 0;
    memLengthMap["csrrci"] = 0;
    bytesMap["csrrci"] = 4;

    // NOTE: Be careful about the format of the disassembled instruction
    opcodeToTypeMap["ecall"] = ExecutionType::Syscall;
    strncpy(syntaxMap["ecall"], "", MAX_OPERANDS);
    memAccessMap["ecall"] = 0;
    memLengthMap["ecall"] = 0;
    bytesMap["ecall"] = 4;

    // NOTE: Be careful about the format of the disassembled instruction
    opcodeToTypeMap["ebreak"] = ExecutionType::Other;
    strncpy(syntaxMap["ebreak"], "", MAX_OPERANDS);
    memAccessMap["ebreak"] = 0;
    memLengthMap["ebreak"] = 0;
    bytesMap["ebreak"] = 4;


    // RV64I instructions

    opcodeToTypeMap["addiw"] = ExecutionType::IntBase;
    strncpy(syntaxMap["addiw"], "WR", MAX_OPERANDS);
    memAccessMap["addiw"] = 0;
    memLengthMap["addiw"] = 0;
    bytesMap["addiw"] = 4;

    opcodeToTypeMap["slliw"] = ExecutionType::IntBase;
    strncpy(syntaxMap["slliw"], "WR", MAX_OPERANDS);
    memAccessMap["slliw"] = 0;
    memLengthMap["slliw"] = 0;
    bytesMap["slliw"] = 4;

    opcodeToTypeMap["srliw"] = ExecutionType::IntBase;
    strncpy(syntaxMap["srliw"], "WR", MAX_OPERANDS);
    memAccessMap["srliw"] = 0;
    memLengthMap["srliw"] = 0;
    bytesMap["srliw"] = 4;

    opcodeToTypeMap["sraiw"] = ExecutionType::IntBase;
    strncpy(syntaxMap["sraiw"], "WR", MAX_OPERANDS);
    memAccessMap["sraiw"] = 0;
    memLengthMap["sraiw"] = 0;
    bytesMap["sraiw"] = 4;

    opcodeToTypeMap["addw"] = ExecutionType::IntBase;
    strncpy(syntaxMap["addw"], "WRR", MAX_OPERANDS);
    memAccessMap["addw"] = 0;
    memLengthMap["addw"] = 0;
    bytesMap["addw"] = 4;

    opcodeToTypeMap["sllw"] = ExecutionType::IntBase;
    strncpy(syntaxMap["sllw"], "WRR", MAX_OPERANDS);
    memAccessMap["sllw"] = 0;
    memLengthMap["sllw"] = 0;
    bytesMap["sllw"] = 4;

    opcodeToTypeMap["srlw"] = ExecutionType::IntBase;
    strncpy(syntaxMap["srlw"], "WRR", MAX_OPERANDS);
    memAccessMap["srlw"] = 0;
    memLengthMap["srlw"] = 0;
    bytesMap["srlw"] = 4;

    opcodeToTypeMap["subw"] = ExecutionType::IntBase;
    strncpy(syntaxMap["subw"], "WRR", MAX_OPERANDS);
    memAccessMap["subw"] = 0;
    memLengthMap["subw"] = 0;
    bytesMap["subw"] = 4;

    opcodeToTypeMap["sraw"] = ExecutionType::IntBase;
    strncpy(syntaxMap["sraw"], "WRR", MAX_OPERANDS);
    memAccessMap["sraw"] = 0;
    memLengthMap["sraw"] = 0;
    bytesMap["sraw"] = 4;

    opcodeToTypeMap["ld"] = ExecutionType::Load;
    strncpy(syntaxMap["ld"], "WR", MAX_OPERANDS);
    memAccessMap["ld"] = 'L';
    memLengthMap["ld"] = 8;
    bytesMap["ld"] = 4;

    opcodeToTypeMap["lwu"] = ExecutionType::Load;
    strncpy(syntaxMap["lwu"], "WR", MAX_OPERANDS);
    memAccessMap["lwu"] = 'L';
    memLengthMap["lwu"] = 4;
    bytesMap["lwu"] = 4;

    opcodeToTypeMap["sd"] = ExecutionType::Store;
    strncpy(syntaxMap["sd"], "RR", MAX_OPERANDS);
    memAccessMap["sd"] = 'S';
    memLengthMap["sd"] = 8;
    bytesMap["sd"] = 4;


    // RVM

    opcodeToTypeMap["mul"] = ExecutionType::IntMul;
    strncpy(syntaxMap["mul"], "WRR", MAX_OPERANDS);
    memAccessMap["mul"] = 0;
    memLengthMap["mul"] = 0;
    bytesMap["mul"] = 4;

    opcodeToTypeMap["mulh"] = ExecutionType::IntMul;
    strncpy(syntaxMap["mulh"], "WRR", MAX_OPERANDS);
    memAccessMap["mulh"] = 0;
    memLengthMap["mulh"] = 0;
    bytesMap["mulh"] = 4;

    opcodeToTypeMap["mulhsu"] = ExecutionType::IntMul;
    strncpy(syntaxMap["mulhsu"], "WRR", MAX_OPERANDS);
    memAccessMap["mulhsu"] = 0;
    memLengthMap["mulhsu"] = 0;
    bytesMap["mulhsu"] = 4;

    opcodeToTypeMap["mulhu"] = ExecutionType::IntMul;
    strncpy(syntaxMap["mulhu"], "WRR", MAX_OPERANDS);
    memAccessMap["mulhu"] = 0;
    memLengthMap["mulhu"] = 0;
    bytesMap["mulhu"] = 4;

    opcodeToTypeMap["mulw"] = ExecutionType::IntMul;
    strncpy(syntaxMap["mulw"], "WRR", MAX_OPERANDS);
    memAccessMap["mulw"] = 0;
    memLengthMap["mulw"] = 0;
    bytesMap["mulw"] = 4;

    opcodeToTypeMap["div"] = ExecutionType::IntDiv;
    strncpy(syntaxMap["div"], "WRR", MAX_OPERANDS);
    memAccessMap["div"] = 0;
    memLengthMap["div"] = 0;
    bytesMap["div"] = 4;

    opcodeToTypeMap["divu"] = ExecutionType::IntDiv;
    strncpy(syntaxMap["divu"], "WRR", MAX_OPERANDS);
    memAccessMap["divu"] = 0;
    memLengthMap["divu"] = 0;
    bytesMap["divu"] = 4;

    opcodeToTypeMap["rem"] = ExecutionType::IntDiv;
    strncpy(syntaxMap["rem"], "WRR", MAX_OPERANDS);
    memAccessMap["rem"] = 0;
    memLengthMap["rem"] = 0;
    bytesMap["rem"] = 4;

    opcodeToTypeMap["remu"] = ExecutionType::IntDiv;
    strncpy(syntaxMap["remu"], "WRR", MAX_OPERANDS);
    memAccessMap["remu"] = 0;
    memLengthMap["remu"] = 0;
    bytesMap["remu"] = 4;

    opcodeToTypeMap["divw"] = ExecutionType::IntDiv;
    strncpy(syntaxMap["divw"], "WRR", MAX_OPERANDS);
    memAccessMap["divw"] = 0;
    memLengthMap["divw"] = 0;
    bytesMap["divw"] = 4;

    opcodeToTypeMap["divuw"] = ExecutionType::IntDiv;
    strncpy(syntaxMap["divuw"], "WRR", MAX_OPERANDS);
    memAccessMap["divuw"] = 0;
    memLengthMap["divuw"] = 0;
    bytesMap["divuw"] = 4;

    opcodeToTypeMap["remw"] = ExecutionType::IntDiv;
    strncpy(syntaxMap["remw"], "WRR", MAX_OPERANDS);
    memAccessMap["remw"] = 0;
    memLengthMap["remw"] = 0;
    bytesMap["remw"] = 4;

    opcodeToTypeMap["remuw"] = ExecutionType::IntDiv;
    strncpy(syntaxMap["remuw"], "WRR", MAX_OPERANDS);
    memAccessMap["remuw"] = 0;
    memLengthMap["remuw"] = 0;
    bytesMap["remuw"] = 4;


    // RVA
    // NOTE: Be careful about the format of the disassembled instruction

    opcodeToTypeMap["lr_w"] = ExecutionType::Atomic;
    strncpy(syntaxMap["lr_w"], "WR", MAX_OPERANDS);
    memAccessMap["lr_w"] = 'L';
    memLengthMap["lr_w"] = 4;
    bytesMap["lr_w"] = 4;

    opcodeToTypeMap["lr_d"] = ExecutionType::Atomic;
    strncpy(syntaxMap["lr_d"], "WR", MAX_OPERANDS);
    memAccessMap["lr_d"] = 'L';
    memLengthMap["lr_d"] = 8;
    bytesMap["lr_d"] = 4;

    opcodeToTypeMap["sc_w"] = ExecutionType::Atomic;
    strncpy(syntaxMap["sc_w"], "WRR", MAX_OPERANDS);
    memAccessMap["sc_w"] = 'S';
    memLengthMap["sc_w"] = 4;
    bytesMap["sc_w"] = 4;

    opcodeToTypeMap["sc_d"] = ExecutionType::Atomic;
    strncpy(syntaxMap["sc_d"], "WRR", MAX_OPERANDS);
    memAccessMap["sc_d"] = 'S';
    memLengthMap["sc_d"] = 8;
    bytesMap["sc_d"] = 4;

    opcodeToTypeMap["amoswap_w"] = ExecutionType::Atomic;
    strncpy(syntaxMap["amoswap_w"], "WRR", MAX_OPERANDS);
    memAccessMap["amoswap_w"] = 'A';
    memLengthMap["amoswap_w"] = 4;
    bytesMap["amoswap_w"] = 4;

    opcodeToTypeMap["amoswap_d"] = ExecutionType::Atomic;
    strncpy(syntaxMap["amoswap_d"], "WRR", MAX_OPERANDS);
    memAccessMap["amoswap_d"] = 'A';
    memLengthMap["amoswap_d"] = 8;
    bytesMap["amoswap_d"] = 4;

    opcodeToTypeMap["amoadd_w"] = ExecutionType::Atomic;
    strncpy(syntaxMap["amoadd_w"], "WRR", MAX_OPERANDS);
    memAccessMap["amoadd_w"] = 'A';
    memLengthMap["amoadd_w"] = 4;
    bytesMap["amoadd_w"] = 4;

    opcodeToTypeMap["amoadd_d"] = ExecutionType::Atomic;
    strncpy(syntaxMap["amoadd_d"], "WRR", MAX_OPERANDS);
    memAccessMap["amoadd_d"] = 'A';
    memLengthMap["amoadd_d"] = 8;
    bytesMap["amoadd_d"] = 4;

    opcodeToTypeMap["amoand_w"] = ExecutionType::Atomic;
    strncpy(syntaxMap["amoand_w"], "WRR", MAX_OPERANDS);
    memAccessMap["amoand_w"] = 'A';
    memLengthMap["amoand_w"] = 4;
    bytesMap["amoand_w"] = 4;

    opcodeToTypeMap["amoand_d"] = ExecutionType::Atomic;
    strncpy(syntaxMap["amoand_d"], "WRR", MAX_OPERANDS);
    memAccessMap["amoand_d"] = 'A';
    memLengthMap["amoand_d"] = 8;
    bytesMap["amoand_d"] = 4;

    opcodeToTypeMap["amoor_w"] = ExecutionType::Atomic;
    strncpy(syntaxMap["amoor_w"], "WRR", MAX_OPERANDS);
    memAccessMap["amoor_w"] = 'A';
    memLengthMap["amoor_w"] = 4;
    bytesMap["amoor_w"] = 4;

    opcodeToTypeMap["amoor_d"] = ExecutionType::Atomic;
    strncpy(syntaxMap["amoor_d"], "WRR", MAX_OPERANDS);
    memAccessMap["amoor_d"] = 'A';
    memLengthMap["amoor_d"] = 8;
    bytesMap["amoor_d"] = 4;

    opcodeToTypeMap["amoxor_w"] = ExecutionType::Atomic;
    strncpy(syntaxMap["amoxor_w"], "WRR", MAX_OPERANDS);
    memAccessMap["amoxor_w"] = 'A';
    memLengthMap["amoxor_w"] = 4;
    bytesMap["amoxor_w"] = 4;

    opcodeToTypeMap["amoxor_d"] = ExecutionType::Atomic;
    strncpy(syntaxMap["amoxor_d"], "WRR", MAX_OPERANDS);
    memAccessMap["amoxor_d"] = 'A';
    memLengthMap["amoxor_d"] = 8;
    bytesMap["amoxor_d"] = 4;

    opcodeToTypeMap["amomax_w"] = ExecutionType::Atomic;
    strncpy(syntaxMap["amomax_w"], "WRR", MAX_OPERANDS);
    memAccessMap["amomax_w"] = 'A';
    memLengthMap["amomax_w"] = 4;
    bytesMap["amomax_w"] = 4;

    opcodeToTypeMap["amomax_d"] = ExecutionType::Atomic;
    strncpy(syntaxMap["amomax_d"], "WRR", MAX_OPERANDS);
    memAccessMap["amomax_d"] = 'A';
    memLengthMap["amomax_d"] = 8;
    bytesMap["amomax_d"] = 4;

    opcodeToTypeMap["amomaxu_w"] = ExecutionType::Atomic;
    strncpy(syntaxMap["amomaxu_w"], "WRR", MAX_OPERANDS);
    memAccessMap["amomaxu_w"] = 'A';
    memLengthMap["amomaxu_w"] = 4;
    bytesMap["amomaxu_w"] = 4;

    opcodeToTypeMap["amomaxu_d"] = ExecutionType::Atomic;
    strncpy(syntaxMap["amomaxu_d"], "WRR", MAX_OPERANDS);
    memAccessMap["amomaxu_d"] = 'A';
    memLengthMap["amomaxu_d"] = 8;
    bytesMap["amomaxu_d"] = 4;

    opcodeToTypeMap["amomin_w"] = ExecutionType::Atomic;
    strncpy(syntaxMap["amomin_w"], "WRR", MAX_OPERANDS);
    memAccessMap["amomin_w"] = 'A';
    memLengthMap["amomin_w"] = 4;
    bytesMap["amomin_w"] = 4;

    opcodeToTypeMap["amomin_d"] = ExecutionType::Atomic;
    strncpy(syntaxMap["amomin_d"], "WRR", MAX_OPERANDS);
    memAccessMap["amomin_d"] = 'A';
    memLengthMap["amomin_d"] = 8;
    bytesMap["amomin_d"] = 4;

    opcodeToTypeMap["amominu_w"] = ExecutionType::Atomic;
    strncpy(syntaxMap["amominu_w"], "WRR", MAX_OPERANDS);
    memAccessMap["amominu_w"] = 'A';
    memLengthMap["amominu_w"] = 4;
    bytesMap["amominu_w"] = 4;

    opcodeToTypeMap["amominu_d"] = ExecutionType::Atomic;
    strncpy(syntaxMap["amominu_d"], "WRR", MAX_OPERANDS);
    memAccessMap["amominu_d"] = 'A';
    memLengthMap["amominu_d"] = 8;
    bytesMap["amominu_d"] = 4;


    // RVF

    opcodeToTypeMap["flw"] = ExecutionType::Load;
    strncpy(syntaxMap["flw"], "WR", MAX_OPERANDS);
    memAccessMap["flw"] = 'L';
    memLengthMap["flw"] = 4;
    bytesMap["flw"] = 4;

    opcodeToTypeMap["fsw"] = ExecutionType::Store;
    strncpy(syntaxMap["fsw"], "RR", MAX_OPERANDS);
    memAccessMap["fsw"] = 'S';
    memLengthMap["fsw"] = 4;
    bytesMap["fsw"] = 4;

    opcodeToTypeMap["fadd_s"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fadd_s"], "WRR", MAX_OPERANDS);
    memAccessMap["fadd_s"] = 0;
    memLengthMap["fadd_s"] = 0;
    bytesMap["fadd_s"] = 4;

    opcodeToTypeMap["fsub_s"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fsub_s"], "WRR", MAX_OPERANDS);
    memAccessMap["fsub_s"] = 0;
    memLengthMap["fsub_s"] = 0;
    bytesMap["fsub_s"] = 4;

    opcodeToTypeMap["fmin_s"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fmin_s"], "WRR", MAX_OPERANDS);
    memAccessMap["fmin_s"] = 0;
    memLengthMap["fmin_s"] = 0;
    bytesMap["fmin_s"] = 4;

    opcodeToTypeMap["fmax_s"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fmax_s"], "WRR", MAX_OPERANDS);
    memAccessMap["fmax_s"] = 0;
    memLengthMap["fmax_s"] = 0;
    bytesMap["fmax_s"] = 4;

    opcodeToTypeMap["fmul_s"] = ExecutionType::FpMul;
    strncpy(syntaxMap["fmul_s"], "WRR", MAX_OPERANDS);
    memAccessMap["fmul_s"] = 0;
    memLengthMap["fmul_s"] = 0;
    bytesMap["fmul_s"] = 4;

    opcodeToTypeMap["fdiv_s"] = ExecutionType::FpDiv;
    strncpy(syntaxMap["fdiv_s"], "WRR", MAX_OPERANDS);
    memAccessMap["fdiv_s"] = 0;
    memLengthMap["fdiv_s"] = 0;
    bytesMap["fdiv_s"] = 4;

    opcodeToTypeMap["fsqrt_s"] = ExecutionType::FpDiv;
    strncpy(syntaxMap["fsqrt_s"], "WR", MAX_OPERANDS);
    memAccessMap["fsqrt_s"] = 0;
    memLengthMap["fsqrt_s"] = 0;
    bytesMap["fsqrt_s"] = 4;

    opcodeToTypeMap["fmadd_s"] = ExecutionType::FpMul;
    strncpy(syntaxMap["fmadd_s"], "WRRR", MAX_OPERANDS);
    memAccessMap["fmadd_s"] = 0;
    memLengthMap["fmadd_s"] = 0;
    bytesMap["fmadd_s"] = 4;

    opcodeToTypeMap["fnmadd_s"] = ExecutionType::FpMul;
    strncpy(syntaxMap["fnmadd_s"], "WRRR", MAX_OPERANDS);
    memAccessMap["fnmadd_s"] = 0;
    memLengthMap["fnmadd_s"] = 0;
    bytesMap["fnmadd_s"] = 4;

    opcodeToTypeMap["fmsub_s"] = ExecutionType::FpMul;
    strncpy(syntaxMap["fmsub_s"], "WRRR", MAX_OPERANDS);
    memAccessMap["fmsub_s"] = 0;
    memLengthMap["fmsub_s"] = 0;
    bytesMap["fmsub_s"] = 4;

    opcodeToTypeMap["fnmsub_s"] = ExecutionType::FpMul;
    strncpy(syntaxMap["fnmsub_s"], "WRRR", MAX_OPERANDS);
    memAccessMap["fnmsub_s"] = 0;
    memLengthMap["fnmsub_s"] = 0;
    bytesMap["fnmsub_s"] = 4;

    opcodeToTypeMap["fcvt_w_s"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fcvt_w_s"], "WR", MAX_OPERANDS);
    memAccessMap["fcvt_w_s"] = 0;
    memLengthMap["fcvt_w_s"] = 0;
    bytesMap["fcvt_w_s"] = 4;

    opcodeToTypeMap["fcvt_wu_s"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fcvt_wu_s"], "WR", MAX_OPERANDS);
    memAccessMap["fcvt_wu_s"] = 0;
    memLengthMap["fcvt_wu_s"] = 0;
    bytesMap["fcvt_wu_s"] = 4;

    opcodeToTypeMap["fcvt_l_s"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fcvt_l_s"], "WR", MAX_OPERANDS);
    memAccessMap["fcvt_l_s"] = 0;
    memLengthMap["fcvt_l_s"] = 0;
    bytesMap["fcvt_l_s"] = 4;

    opcodeToTypeMap["fcvt_lu_s"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fcvt_lu_s"], "WR", MAX_OPERANDS);
    memAccessMap["fcvt_lu_s"] = 0;
    memLengthMap["fcvt_lu_s"] = 0;
    bytesMap["fcvt_lu_s"] = 4;

    opcodeToTypeMap["fcvt_s_w"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fcvt_s_w"], "WR", MAX_OPERANDS);
    memAccessMap["fcvt_s_w"] = 0;
    memLengthMap["fcvt_s_w"] = 0;
    bytesMap["fcvt_s_w"] = 4;

    opcodeToTypeMap["fcvt_s_wu"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fcvt_s_wu"], "WR", MAX_OPERANDS);
    memAccessMap["fcvt_s_wu"] = 0;
    memLengthMap["fcvt_s_wu"] = 0;
    bytesMap["fcvt_s_wu"] = 4;

    opcodeToTypeMap["fcvt_s_l"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fcvt_s_l"], "WR", MAX_OPERANDS);
    memAccessMap["fcvt_s_l"] = 0;
    memLengthMap["fcvt_s_l"] = 0;
    bytesMap["fcvt_s_l"] = 4;

    opcodeToTypeMap["fcvt_s_lu"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fcvt_s_lu"], "WR", MAX_OPERANDS);
    memAccessMap["fcvt_s_lu"] = 0;
    memLengthMap["fcvt_s_lu"] = 0;
    bytesMap["fcvt_s_lu"] = 4;

    opcodeToTypeMap["fsgnj_s"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fsgnj_s"], "WRR", MAX_OPERANDS);
    memAccessMap["fsgnj_s"] = 0;
    memLengthMap["fsgnj_s"] = 0;
    bytesMap["fsgnj_s"] = 4;

    opcodeToTypeMap["fsgnjn_s"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fsgnjn_s"], "WRR", MAX_OPERANDS);
    memAccessMap["fsgnjn_s"] = 0;
    memLengthMap["fsgnjn_s"] = 0;
    bytesMap["fsgnjn_s"] = 4;

    opcodeToTypeMap["fsgnjx_s"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fsgnjx_s"], "WRR", MAX_OPERANDS);
    memAccessMap["fsgnjx_s"] = 0;
    memLengthMap["fsgnjx_s"] = 0;
    bytesMap["fsgnjx_s"] = 4;

    opcodeToTypeMap["fmv_x_s"] = ExecutionType::FpBase; // AKA fmv_x_w
    strncpy(syntaxMap["fmv_x_s"], "WR", MAX_OPERANDS);
    memAccessMap["fmv_x_s"] = 0;
    memLengthMap["fmv_x_s"] = 0;
    bytesMap["fmv_x_s"] = 4;

    opcodeToTypeMap["fmv_s_x"] = ExecutionType::FpBase; // AKA fmv_w_x
    strncpy(syntaxMap["fmv_s_x"], "WR", MAX_OPERANDS);
    memAccessMap["fmv_s_x"] = 0;
    memLengthMap["fmv_s_x"] = 0;
    bytesMap["fmv_s_x"] = 4;

    opcodeToTypeMap["flt_s"] = ExecutionType::FpBase;
    strncpy(syntaxMap["flt_s"], "WRR", MAX_OPERANDS);
    memAccessMap["flt_s"] = 0;
    memLengthMap["flt_s"] = 0;
    bytesMap["flt_s"] = 4;

    opcodeToTypeMap["fle_s"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fle_s"], "WRR", MAX_OPERANDS);
    memAccessMap["fle_s"] = 0;
    memLengthMap["fle_s"] = 0;
    bytesMap["fle_s"] = 4;

    opcodeToTypeMap["feq_s"] = ExecutionType::FpBase;
    strncpy(syntaxMap["feq_s"], "WRR", MAX_OPERANDS);
    memAccessMap["feq_s"] = 0;
    memLengthMap["feq_s"] = 0;
    bytesMap["feq_s"] = 4;

    opcodeToTypeMap["fclass_s"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fclass_s"], "WR", MAX_OPERANDS);
    memAccessMap["fclass_s"] = 0;
    memLengthMap["fclass_s"] = 0;
    bytesMap["fclass_s"] = 4;


    // RVD

    opcodeToTypeMap["fld"] = ExecutionType::Load;
    strncpy(syntaxMap["fld"], "WR", MAX_OPERANDS);
    memAccessMap["fld"] = 'L';
    memLengthMap["fld"] = 8;
    bytesMap["fld"] = 4;

    opcodeToTypeMap["fsd"] = ExecutionType::Store;
    strncpy(syntaxMap["fsd"], "RR", MAX_OPERANDS);
    memAccessMap["fsd"] = 'S';
    memLengthMap["fsd"] = 8;
    bytesMap["fsd"] = 4;

    opcodeToTypeMap["fadd_d"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fadd_d"], "WRR", MAX_OPERANDS);
    memAccessMap["fadd_d"] = 0;
    memLengthMap["fadd_d"] = 0;
    bytesMap["fadd_d"] = 4;

    opcodeToTypeMap["fsub_d"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fsub_d"], "WRR", MAX_OPERANDS);
    memAccessMap["fsub_d"] = 0;
    memLengthMap["fsub_d"] = 0;
    bytesMap["fsub_d"] = 4;

    opcodeToTypeMap["fmin_d"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fmin_d"], "WRR", MAX_OPERANDS);
    memAccessMap["fmin_d"] = 0;
    memLengthMap["fmin_d"] = 0;
    bytesMap[""] = 4;

    opcodeToTypeMap["fmax_d"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fmax_d"], "WRR", MAX_OPERANDS);
    memAccessMap["fmax_d"] = 0;
    memLengthMap["fmax_d"] = 0;
    bytesMap["fmax_d"] = 4;

    opcodeToTypeMap["fmul_d"] = ExecutionType::FpMul;
    strncpy(syntaxMap["fmul_d"], "WRR", MAX_OPERANDS);
    memAccessMap["fmul_d"] = 0;
    memLengthMap["fmul_d"] = 0;
    bytesMap["fmul_d"] = 4;

    opcodeToTypeMap["fdiv_d"] = ExecutionType::FpDiv;
    strncpy(syntaxMap["fdiv_d"], "WRR", MAX_OPERANDS);
    memAccessMap["fdiv_d"] = 0;
    memLengthMap["fdiv_d"] = 0;
    bytesMap["fdiv_d"] = 4;

    opcodeToTypeMap["fsqrt_d"] = ExecutionType::FpDiv;
    strncpy(syntaxMap["fsqrt_d"], "WR", MAX_OPERANDS);
    memAccessMap["fsqrt_d"] = 0;
    memLengthMap["fsqrt_d"] = 0;
    bytesMap["fsqrt_d"] = 4;

    opcodeToTypeMap["fmadd_d"] = ExecutionType::FpMul;
    strncpy(syntaxMap["fmadd_d"], "WRRR", MAX_OPERANDS);
    memAccessMap["fmadd_d"] = 0;
    memLengthMap["fmadd_d"] = 0;
    bytesMap["fmadd_d"] = 4;

    opcodeToTypeMap["fnmadd_d"] = ExecutionType::FpMul;
    strncpy(syntaxMap["fnmadd_d"], "WRRR", MAX_OPERANDS);
    memAccessMap["fnmadd_d"] = 0;
    memLengthMap["fnmadd_d"] = 0;
    bytesMap["fnmadd_d"] = 4;

    opcodeToTypeMap["fmsub_d"] = ExecutionType::FpMul;
    strncpy(syntaxMap["fmsub_d"], "WRRR", MAX_OPERANDS);
    memAccessMap["fmsub_d"] = 0;
    memLengthMap["fmsub_d"] = 0;
    bytesMap["fmsub_d"] = 4;

    opcodeToTypeMap["fnmsub_d"] = ExecutionType::FpMul;
    strncpy(syntaxMap["fnmsub_d"], "WRRR", MAX_OPERANDS);
    memAccessMap["fnmsub_d"] = 0;
    memLengthMap["fnmsub_d"] = 0;
    bytesMap["fnmsub_d"] = 4;

    opcodeToTypeMap["fcvt_s_d"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fcvt_s_d"], "WR", MAX_OPERANDS);
    memAccessMap["fcvt_s_d"] = 0;
    memLengthMap["fcvt_s_d"] = 0;
    bytesMap["fcvt_s_d"] = 4;

    opcodeToTypeMap["fcvt_d_s"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fcvt_d_s"], "WR", MAX_OPERANDS);
    memAccessMap["fcvt_d_s"] = 0;
    memLengthMap["fcvt_d_s"] = 0;
    bytesMap["fcvt_d_s"] = 4;

    opcodeToTypeMap["fcvt_w_d"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fcvt_w_d"], "WR", MAX_OPERANDS);
    memAccessMap["fcvt_w_d"] = 0;
    memLengthMap["fcvt_w_d"] = 0;
    bytesMap["fcvt_w_d"] = 4;

    opcodeToTypeMap["fcvt_wu_d"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fcvt_wu_d"], "WR", MAX_OPERANDS);
    memAccessMap["fcvt_wu_d"] = 0;
    memLengthMap["fcvt_wu_d"] = 0;
    bytesMap["fcvt_wu_d"] = 4;

    opcodeToTypeMap["fcvt_l_d"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fcvt_l_d"], "WR", MAX_OPERANDS);
    memAccessMap["fcvt_l_d"] = 0;
    memLengthMap["fcvt_l_d"] = 0;
    bytesMap["fcvt_l_d"] = 4;

    opcodeToTypeMap["fcvt_lu_d"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fcvt_lu_d"], "WR", MAX_OPERANDS);
    memAccessMap["fcvt_lu_d"] = 0;
    memLengthMap["fcvt_lu_d"] = 0;
    bytesMap["fcvt_lu_d"] = 4;

    opcodeToTypeMap["fcvt_d_w"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fcvt_d_w"], "WR", MAX_OPERANDS);
    memAccessMap["fcvt_d_w"] = 0;
    memLengthMap["fcvt_d_w"] = 0;
    bytesMap["fcvt_d_w"] = 4;

    opcodeToTypeMap["fcvt_d_wu"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fcvt_d_wu"], "WR", MAX_OPERANDS);
    memAccessMap["fcvt_d_wu"] = 0;
    memLengthMap["fcvt_d_wu"] = 0;
    bytesMap["fcvt_d_wu"] = 4;

    opcodeToTypeMap["fcvt_d_l"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fcvt_d_l"], "WR", MAX_OPERANDS);
    memAccessMap["fcvt_d_l"] = 0;
    memLengthMap["fcvt_d_l"] = 0;
    bytesMap["fcvt_d_l"] = 4;

    opcodeToTypeMap["fcvt_d_lu"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fcvt_d_lu"], "WR", MAX_OPERANDS);
    memAccessMap["fcvt_d_lu"] = 0;
    memLengthMap["fcvt_d_lu"] = 0;
    bytesMap["fcvt_d_lu"] = 4;

    opcodeToTypeMap["fsgnj_d"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fsgnj_d"], "WRR", MAX_OPERANDS);
    memAccessMap["fsgnj_d"] = 0;
    memLengthMap["fsgnj_d"] = 0;
    bytesMap["fsgnj_d"] = 4;

    opcodeToTypeMap["fsgnjn_d"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fsgnjn_d"], "WRR", MAX_OPERANDS);
    memAccessMap["fsgnjn_d"] = 0;
    memLengthMap["fsgnjn_d"] = 0;
    bytesMap["fsgnjn_d"] = 4;

    opcodeToTypeMap["fsgnjx_d"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fsgnjx_d"], "WRR", MAX_OPERANDS);
    memAccessMap["fsgnjx_d"] = 0;
    memLengthMap["fsgnjx_d"] = 0;
    bytesMap["fsgnjx_d"] = 4;

    opcodeToTypeMap["fmv_x_d"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fmv_x_d"], "WR", MAX_OPERANDS);
    memAccessMap["fmv_x_d"] = 0;
    memLengthMap["fmv_x_d"] = 0;
    bytesMap["fmv_x_d"] = 4;

    opcodeToTypeMap["fmv_d_x"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fmv_d_x"], "WR", MAX_OPERANDS);
    memAccessMap["fmv_d_x"] = 0;
    memLengthMap["fmv_d_x"] = 0;
    bytesMap["fmv_d_x"] = 4;

    opcodeToTypeMap["fclass_d"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fclass_d"], "WR", MAX_OPERANDS);
    memAccessMap["fclass_d"] = 0;
    memLengthMap["fclass_d"] = 0;
    bytesMap["fclass_d"] = 4;

    opcodeToTypeMap["flt_d"] = ExecutionType::FpBase;
    strncpy(syntaxMap["flt_d"], "WRR", MAX_OPERANDS);
    memAccessMap["flt_d"] = 0;
    memLengthMap["flt_d"] = 0;
    bytesMap["flt_d"] = 4;

    opcodeToTypeMap["fle_d"] = ExecutionType::FpBase;
    strncpy(syntaxMap["fle_d"], "WRR", MAX_OPERANDS);
    memAccessMap["fle_d"] = 0;
    memLengthMap["fle_d"] = 0;
    bytesMap["fle_d"] = 4;

    opcodeToTypeMap["feq_d"] = ExecutionType::FpBase;
    strncpy(syntaxMap["feq_d"], "WRR", MAX_OPERANDS);
    memAccessMap["feq_d"] = 0;
    memLengthMap["feq_d"] = 0;
    bytesMap["feq_d"] = 4;

    
    // RVC

    opcodeToTypeMap["c_lwsp"] = ExecutionType::Load;
    strncpy(syntaxMap["c_lwsp"], "WR", MAX_OPERANDS);
    memAccessMap["c_lwsp"] = 'L';
    memLengthMap["c_lwsp"] = 4;
    bytesMap["c_lwsp"] = 2;

    opcodeToTypeMap["c_ldsp"] = ExecutionType::Load;
    strncpy(syntaxMap["c_ldsp"], "WR", MAX_OPERANDS);
    memAccessMap["c_ldsp"] = 'L';
    memLengthMap["c_ldsp"] = 8;
    bytesMap["c_ldsp"] = 2;

    opcodeToTypeMap["c_fldsp"] = ExecutionType::Load;
    strncpy(syntaxMap["c_fldsp"], "WR", MAX_OPERANDS);
    memAccessMap["c_fldsp"] = 'L';
    memLengthMap["c_fldsp"] = 8;
    bytesMap["c_fldsp"] = 2;

    opcodeToTypeMap["c_swsp"] = ExecutionType::Store;
    strncpy(syntaxMap["c_swsp"], "RR", MAX_OPERANDS);
    memAccessMap["c_swsp"] = 'S';
    memLengthMap["c_swsp"] = 4;
    bytesMap["c_swsp"] = 2;

    opcodeToTypeMap["c_sdsp"] = ExecutionType::Store;
    strncpy(syntaxMap["c_sdsp"], "RR", MAX_OPERANDS);
    memAccessMap["c_sdsp"] = 'S';
    memLengthMap["c_sdsp"] = 8;
    bytesMap["c_sdsp"] = 2;

    opcodeToTypeMap["c_fsdsp"] = ExecutionType::Store;
    strncpy(syntaxMap["c_fsdsp"], "RR", MAX_OPERANDS);
    memAccessMap["c_fsdsp"] = 'S';
    memLengthMap["c_fsdsp"] = 8;
    bytesMap["c_fsdsp"] = 2;

    opcodeToTypeMap["c_lw"] = ExecutionType::Load;
    strncpy(syntaxMap["c_lw"], "WR", MAX_OPERANDS);
    memAccessMap["c_lw"] = 'L';
    memLengthMap["c_lw"] = 4;
    bytesMap["c_lw"] = 2;

    opcodeToTypeMap["c_ld"] = ExecutionType::Load;
    strncpy(syntaxMap["c_ld"], "WR", MAX_OPERANDS);
    memAccessMap["c_ld"] = 'L';
    memLengthMap["c_ld"] = 8;
    bytesMap["c_ld"] = 2;

    opcodeToTypeMap["c_fld"] = ExecutionType::Load;
    strncpy(syntaxMap["c_fld"], "WR", MAX_OPERANDS);
    memAccessMap["c_fld"] = 'L';
    memLengthMap["c_fld"] = 8;
    bytesMap["c_fld"] = 2;

    opcodeToTypeMap["c_sw"] = ExecutionType::Store;
    strncpy(syntaxMap["c_sw"], "RR", MAX_OPERANDS);
    memAccessMap["c_sw"] = 'S';
    memLengthMap["c_sw"] = 4;
    bytesMap["c_sw"] = 2;

    opcodeToTypeMap["c_sd"] = ExecutionType::Store;
    strncpy(syntaxMap["c_sd"], "RR", MAX_OPERANDS);
    memAccessMap["c_sd"] = 'S';
    memLengthMap["c_sd"] = 8;
    bytesMap["c_sd"] = 2;

    opcodeToTypeMap["c_fsd"] = ExecutionType::Store;
    strncpy(syntaxMap["c_fsd"], "RR", MAX_OPERANDS);
    memAccessMap["c_fsd"] = 'S';
    memLengthMap["c_fsd"] = 8;
    bytesMap["c_fsd"] = 2;

    opcodeToTypeMap["c_j"] = ExecutionType::BranchUncond;
    strncpy(syntaxMap["c_j"], "W", MAX_OPERANDS);
    memAccessMap["c_j"] = 0;
    memLengthMap["c_j"] = 0;
    bytesMap["c_j"] = 2;

    opcodeToTypeMap["c_jr"] = ExecutionType::BranchUncond;
    strncpy(syntaxMap["c_jr"], "WR", MAX_OPERANDS);
    memAccessMap["c_jr"] = 0;
    memLengthMap["c_jr"] = 0;
    bytesMap["c_jr"] = 2;

    opcodeToTypeMap["c_jalr"] = ExecutionType::BranchUncond;
    strncpy(syntaxMap["c_jalr"], "WR", MAX_OPERANDS);
    memAccessMap["c_jalr"] = 0;
    memLengthMap["c_jalr"] = 0;
    bytesMap["c_jalr"] = 2;

    opcodeToTypeMap["c_beqz"] = ExecutionType::BranchCond;
    strncpy(syntaxMap["c_beqz"], "RR", MAX_OPERANDS);
    memAccessMap["c_beqz"] = 0;
    memLengthMap["c_beqz"] = 0;
    bytesMap["c_beqz"] = 2;

    opcodeToTypeMap["c_bnez"] = ExecutionType::BranchCond;
    strncpy(syntaxMap["c_bnez"], "RR", MAX_OPERANDS);
    memAccessMap["c_bnez"] = 0;
    memLengthMap["c_bnez"] = 0;
    bytesMap["c_bnez"] = 2;

    opcodeToTypeMap["c_li"] = ExecutionType::IntBase;
    strncpy(syntaxMap["c_li"], "WR", MAX_OPERANDS);
    memAccessMap["c_li"] = 0;
    memLengthMap["c_li"] = 0;
    bytesMap["c_li"] = 2;

    opcodeToTypeMap["c_lui"] = ExecutionType::IntBase;
    strncpy(syntaxMap["c_lui"], "W", MAX_OPERANDS);
    memAccessMap["c_lui"] = 0;
    memLengthMap["c_lui"] = 0;
    bytesMap["c_lui"] = 2;

    opcodeToTypeMap["c_addi"] = ExecutionType::IntBase;
    strncpy(syntaxMap["c_addi"], "WR", MAX_OPERANDS);
    memAccessMap["c_addi"] = 0;
    memLengthMap["c_addi"] = 0;
    bytesMap["c_addi"] = 2;

    opcodeToTypeMap["c_addiw"] = ExecutionType::IntBase;
    strncpy(syntaxMap["c_addiw"], "WR", MAX_OPERANDS);
    memAccessMap["c_addiw"] = 0;
    memLengthMap["c_addiw"] = 0;
    bytesMap["c_addiw"] = 2;

    opcodeToTypeMap["c_addi16sp"] = ExecutionType::IntBase;
    strncpy(syntaxMap["c_addi16sp"], "WR", MAX_OPERANDS);
    memAccessMap["c_addi16sp"] = 0;
    memLengthMap["c_addi16sp"] = 0;
    bytesMap["c_addi16sp"] = 2;

    opcodeToTypeMap["c_addi4spn"] = ExecutionType::IntBase;
    strncpy(syntaxMap["c_addi4spn"], "WR", MAX_OPERANDS);
    memAccessMap["c_addi4spn"] = 0;
    memLengthMap["c_addi4spn"] = 0;
    bytesMap["c_addi4spn"] = 2;

    opcodeToTypeMap["c_slli"] = ExecutionType::IntBase;
    strncpy(syntaxMap["c_slli"], "WR", MAX_OPERANDS);
    memAccessMap["c_slli"] = 0;
    memLengthMap["c_slli"] = 0;
    bytesMap["c_slli"] = 2;

    opcodeToTypeMap["c_srli"] = ExecutionType::IntBase;
    strncpy(syntaxMap["c_srli"], "WR", MAX_OPERANDS);
    memAccessMap["c_srli"] = 0;
    memLengthMap["c_srli"] = 0;
    bytesMap["c_srli"] = 2;

    opcodeToTypeMap["c_srai"] = ExecutionType::IntBase;
    strncpy(syntaxMap["c_srai"], "WR", MAX_OPERANDS);
    memAccessMap["c_srai"] = 0;
    memLengthMap["c_srai"] = 0;
    bytesMap["c_srai"] = 2;

    opcodeToTypeMap["c_andi"] = ExecutionType::IntBase;
    strncpy(syntaxMap["c_andi"], "WR", MAX_OPERANDS);
    memAccessMap["c_andi"] = 0;
    memLengthMap["c_andi"] = 0;
    bytesMap["c_andi"] = 2;

    opcodeToTypeMap["c_mv"] = ExecutionType::IntBase;
    strncpy(syntaxMap["c_mv"], "WRR", MAX_OPERANDS);
    memAccessMap["c_mv"] = 0;
    memLengthMap["c_mv"] = 0;
    bytesMap["c_mv"] = 2;

    opcodeToTypeMap["c_add"] = ExecutionType::IntBase;
    strncpy(syntaxMap["c_add"], "WRR", MAX_OPERANDS);
    memAccessMap["c_add"] = 0;
    memLengthMap["c_add"] = 0;
    bytesMap["c_add"] = 2;

    opcodeToTypeMap["c_and"] = ExecutionType::IntBase;
    strncpy(syntaxMap["c_and"], "WRR", MAX_OPERANDS);
    memAccessMap["c_and"] = 0;
    memLengthMap["c_and"] = 0;
    bytesMap["c_and"] = 2;

    opcodeToTypeMap["c_or"] = ExecutionType::IntBase;
    strncpy(syntaxMap["c_or"], "WRR", MAX_OPERANDS);
    memAccessMap["c_or"] = 0;
    memLengthMap["c_or"] = 0;
    bytesMap["c_or"] = 2;

    opcodeToTypeMap["c_xor"] = ExecutionType::IntBase;
    strncpy(syntaxMap["c_xor"], "WRR", MAX_OPERANDS);
    memAccessMap["c_xor"] = 0;
    memLengthMap["c_xor"] = 0;
    bytesMap["c_xor"] = 2;

    opcodeToTypeMap["c_sub"] = ExecutionType::IntBase;
    strncpy(syntaxMap["c_sub"], "WRR", MAX_OPERANDS);
    memAccessMap["c_sub"] = 0;
    memLengthMap["c_sub"] = 0;
    bytesMap["c_sub"] = 2;

    opcodeToTypeMap["c_addw"] = ExecutionType::IntBase;
    strncpy(syntaxMap["c_addw"], "WRR", MAX_OPERANDS);
    memAccessMap["c_addw"] = 0;
    memLengthMap["c_addw"] = 0;
    bytesMap["c_addw"] = 2;

    opcodeToTypeMap["c_subw"] = ExecutionType::IntBase;
    strncpy(syntaxMap["c_subw"], "WRR", MAX_OPERANDS);
    memAccessMap["c_subw"] = 0;
    memLengthMap["c_subw"] = 0;
    bytesMap["c_subw"] = 2;

    opcodeToTypeMap["c_nop"] = ExecutionType::IntBase;
    strncpy(syntaxMap["c_nop"], "WR", MAX_OPERANDS);
    memAccessMap["c_nop"] = 0;
    memLengthMap["c_nop"] = 0;
    bytesMap["c_nop"] = 2;

    opcodeToTypeMap["c_ebreak"] = ExecutionType::Other;
    strncpy(syntaxMap["c_ebreak"], "", MAX_OPERANDS);
    memAccessMap["c_ebreak"] = 0;
    memLengthMap["c_ebreak"] = 0;
    bytesMap["c_ebreak"] = 2;


    // RVN

    opcodeToTypeMap["uret"] = ExecutionType::Other;
    strncpy(syntaxMap["uret"], "", MAX_OPERANDS);
    memAccessMap["uret"] = 0;
    memLengthMap["uret"] = 0;
    bytesMap["uret"] = 4;

    opcodeToTypeMap["sret"] = ExecutionType::Other;
    strncpy(syntaxMap["sret"], "", MAX_OPERANDS);
    memAccessMap["sret"] = 0;
    memLengthMap["sret"] = 0;
    bytesMap["sret"] = 4;

    opcodeToTypeMap["mret"] = ExecutionType::Other;
    strncpy(syntaxMap["mret"], "", MAX_OPERANDS);
    memAccessMap["mret"] = 0;
    memLengthMap["mret"] = 0;
    bytesMap["mret"] = 4;
}
