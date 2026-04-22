#include "zon_vm.hpp"
#include <iostream>

namespace zonvm {
    void VM::run() {
        pc = code.data();
        word* end = pc + code.size();
        bool running = true;

        while (pc < end && running) {
            word instruction = *pc++;

            byte opcode = instruction & 0x7F;
            int rd      = (instruction >> 7) & 0x1F;
            int funct3  = (instruction >> 12) & 0x7;
            int rs1     = (instruction >> 15) & 0x1F;
            int rs2     = (instruction >> 20) & 0x1F;
            int funct7  = (instruction >> 25) & 0x7F;

            switch (opcode) {
                case OP_IMM: {
                    int32_t immI = (instruction >> 20) & 0xFFF;
                    if (immI & 0x800) immI |= 0xFFFFF000;
                    int32_t v1 = read_reg(rs1);

                    if (funct3 == ADD_SUB) write_reg(rd, v1 + immI);
                    else if (funct3 == SLT_SLTI) write_reg(rd, (v1 < immI) ? 1 : 0);
                    else if (funct3 == SLTU_SLTIU) write_reg(rd, ((uint32_t)v1 < (uint32_t)immI) ? 1 : 0);
                    else if (funct3 == XOR_XORI) write_reg(rd, v1 ^ immI);
                    else if (funct3 == OR_ORI) write_reg(rd, v1 | immI);
                    else if (funct3 == AND_ANDI) write_reg(rd, v1 & immI);
                    break;
                }

                case OP: {
                    int32_t v1 = read_reg(rs1);
                    int32_t v2 = read_reg(rs2);

                     if (funct7 == M_EXT) {
                        if (funct3 == MUL) {
                            write_reg(rd, v1 * v2);
                        } else if (funct3 == DIV) {
                            if (v2 != 0) write_reg(rd, v1 / v2);
                            else write_reg(rd, 0);
                        } else if (funct3 == REM) {
                            if (v2 != 0) write_reg(rd, v1 % v2);
                            else write_reg(rd, 0);
                        }
                    } else if (funct7 == STANDARD) {
                        if (funct3 == ADD_SUB) write_reg(rd, v1 + v2);
                        else if (funct3 == SLT_SLTI) write_reg(rd, (v1 < v2) ? 1 : 0);
                        else if (funct3 == SLTU_SLTIU) write_reg(rd, ((uint32_t)v1 < (uint32_t)v2) ? 1 : 0);
                        else if (funct3 == XOR_XORI) write_reg(rd, v1 ^ v2);
                        else if (funct3 == OR_ORI) write_reg(rd, v1 | v2);
                        else if (funct3 == AND_ANDI) write_reg(rd, v1 & v2);
                        
                    } else if (funct7 == ALT) {
                        if (funct3 == ADD_SUB) {
                            write_reg(rd, v1 - v2);
                        }
                    }
                    break;
                }

                case OP_B: {
                    int32_t b12   = (instruction >> 31) & 0x1;
                    int32_t b11   = (instruction >> 7)  & 0x1;
                    int32_t b10_5 = (instruction >> 25) & 0x3F;
                    int32_t b4_1  = (instruction >> 8)  & 0xF;
                    
                    int32_t immB = (b12 << 12) | (b11 << 11) | (b10_5 << 5) | (b4_1 << 1);
                    if (immB & 0x1000) immB |= 0xFFFFE000;

                    int32_t v1 = read_reg(rs1);
                    int32_t v2 = read_reg(rs2);
                    bool take = false;

                    if (funct3 == BEQ) take = (v1 == v2);
                    else if (funct3 == BNE) take = (v1 != v2);
                    else if (funct3 == BLT) take = (v1 < v2);
                    else if (funct3 == BGE) take = (v1 >= v2);
                    else if (funct3 == BLTU) take = ((uint32_t)v1 >= (uint32_t)v2);
                    else if (funct3 == BGEU) take = ((uint32_t)v1 >= (uint32_t)v2);

                    if (take) {
                        pc = (pc - 1) + (immB / 4);
                    }
                    break;
                }

                case JAL: {
                    uint32_t off20     = (instruction >> 31) & 0x1;
                    uint32_t off19_12 = (instruction >> 12) & 0xFF;
                    uint32_t off11     = (instruction >> 20) & 0x1;
                    uint32_t off10_1  = (instruction >> 21) & 0x3FF;

                    int32_t offset = (off20 << 20) | (off19_12 << 12) | (off11 << 11) | (off10_1 << 1);

                    if (off20) {
                        offset |= 0xFFE00000; 
                    }

                    pc = (pc - 1) + (offset / 4);
                    break;
                }

                case ECALL: {
                    int service = read_reg(17);
                    if (service == 10) running = false;
                    else if (service == 1) {
                        std::cout << "[Debug Vm Output]: " << read_reg(10) << std::endl;
                    }
                    break;
                }

                default:
                    std::cerr << "Opcode desconocido: " << (int)opcode << std::endl;
                    return;
            }
        }
    }
}