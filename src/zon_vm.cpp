#include "zon_vm.hpp"
#include <iostream>
#include <bit>

namespace zonvm {
    void VM::run() {
        pc = code.data();
        word* end = pc + code.size();
        
        static void* dispatch_table[128] = { &&unknown_op };
        dispatch_table[OP_IMM] = &&exec_op_imm;
        dispatch_table[OP]     = &&exec_op;
        dispatch_table[OP_B]   = &&exec_op_b;
        dispatch_table[JAL]    = &&exec_jal;
        dispatch_table[ECALL]  = &&exec_ecall;
        dispatch_table[LUI]    = &&exec_lui;
        dispatch_table[OP_F]   = &&exec_op_f; 

        #define DISPATCH() \
            if (pc >= end) return; \
            goto *dispatch_table[*pc & 0x7F]

        DISPATCH();

        exec_op_imm: {
            word inst = *pc++;
            byte rd = (inst >> 7) & 0x1F;
            byte funct3 = (inst >> 12) & 0x7;
            int64_t v1 = regs[((inst >> 15) & 0x1F)];
            int64_t immI = sext(((inst >> 20) & 0xFFF), 12);

            if (funct3 == ADD_SUB) regs[rd] = v1 + immI;
            else if (funct3 == SLT_SLTI) regs[rd] = (v1 < immI) ? 1 : 0;
            else if (funct3 == SLTU_SLTIU) regs[rd]= ((uint64_t)v1 < (uint64_t)immI) ? 1 : 0;
            else if (funct3 == XOR_XORI) regs[rd] = v1 ^ immI;
            else if (funct3 == OR_ORI) regs[rd] = v1 | immI;
            else if (funct3 == AND_ANDI) regs[rd] = v1 & immI;

            regs[0] = 0;
            
            DISPATCH();
        }

        exec_lui: {
            word inst = *pc++;
            byte rd = (inst >> 7) & 0x1F;
            regs[rd] = (int64_t)(int32_t)(inst & 0xFFFFF000);
            regs[0] = 0;

            DISPATCH();
        }


        exec_op: {
            word inst = *pc++;
            byte opcode = inst & 0x7F;
            byte rd      = (inst >> 7) & 0x1F;
            byte funct3  = (inst >> 12) & 0x7;
            int64_t v1 = regs[((inst >> 15) & 0x1F)];
            int64_t v2 = regs[((inst >> 20) & 0x1F)];
            byte funct7  = (inst >> 25) & 0x7F;

            if (funct7 == M_EXT) {
                if (funct3 == MUL) regs[rd] = v1 * v2;
                else if (funct3 == DIV) {
                    if (v2 != 0) regs[rd] = v1 / v2;
                    else regs[rd] = 0;
                } else if (funct3 == REM) {
                    if (v2 != 0) regs[rd] = v1 % v2;
                    else regs[rd] = 0;
                }
            } else if (funct7 == STANDARD) {
                if (funct3 == ADD_SUB) regs[rd] = v1 + v2;
                else if (funct3 == SLT_SLTI) regs[rd] = (v1 < v2) ? 1 : 0;
                else if (funct3 == SLTU_SLTIU) regs[rd] = ((uint64_t)v1 < (uint64_t)v2) ? 1 : 0;
                else if (funct3 == XOR_XORI) regs[rd] = v1 ^ v2;
                else if (funct3 == OR_ORI) regs[rd] = v1 | v2;
                else if (funct3 == AND_ANDI) regs[rd] = v1 & v2;
                
            } else if (funct7 == ALT) {
                if (funct3 == ADD_SUB) regs[rd] = v1 - v2;
            }

            regs[0] = 0;

            DISPATCH();
        }

        exec_op_f: {
            word inst = *pc++;
            byte rd     = (inst >> 7)  & 0x1F;
            byte rm     = (inst >> 12) & 0x07;
            byte rs1    = (inst >> 15) & 0x1F;
            byte rs2    = (inst >> 20) & 0x1F;
            byte funct7 = (inst >> 25) & 0x7F;
 
            if (funct7 == FCVT_S_W) {
                float f = static_cast<float>((int32_t)regs[rs1]);
                fregs[rd] = (double)f;

            } else if (funct7 == FSGNJ_S) { 
                uint32_t f1 = std::bit_cast<uint32_t>((float)fregs[rs1]);
                uint32_t f2 = std::bit_cast<uint32_t>((float)fregs[rs2]);
                uint32_t res_bits;

                if (rm == 0x00) res_bits = (f1 & 0x7FFFFFFF) | (f2 & 0x80000000);
                else if (rm == 0x01) res_bits = (f1 & 0x7FFFFFFF) | ((f2 & 0x80000000) ^ 0x80000000);

                fregs[rd] = (double)std::bit_cast<float>(res_bits);

            } else if (funct7 == FMV_W_X) {
                float f = std::bit_cast<float>((uint32_t)regs[rs1]);
                fregs[rd] = (double)f;

            } else if (funct7 == STANDARD) {
                float f1 = (float)fregs[rs1];
                float f2 = (float)fregs[rs2];
                fregs[rd] = (double)(f1 + f2);

            } else if (funct7 == FSUB_S) {
                float f1 = (float)fregs[rs1];
                float f2 = (float)fregs[rs2];
                fregs[rd] = (double)(f1 - f2);

            } else if (funct7 == FMUL_S) {
                float f1 = (float)fregs[rs1];
                float f2 = (float)fregs[rs2];
                fregs[rd] = (double)(f1 * f2);
            } else if (funct7 == FDIV_S) {
                float f1 = (float)fregs[rs1];
                float f2 = (float)fregs[rs2];
                if (f2 == 0.0) fregs[rd] = 0.0;
                else fregs[rd] = (double)(f1 / f2);

            } else if (funct7 == FCOMP_S) {
                float f1 = (float)fregs[rs1];
                float f2 = (float)fregs[rs2];
                int64_t res;
                if (rm == 0x00) res = (f1 <= f2) ? 1 : 0;
                if (rm == 0x01) res = (f1 < f2) ? 1 : 0;
                if (rm == 0x02) res = (f1 == f2) ? 1 : 0;
                regs[rd] = res;
            } 
         
            DISPATCH();
        }

        exec_op_b: {
            word inst = *pc++;
            uint64_t b12   = (inst >> 31) & 0x1;
            uint64_t b11   = (inst >> 7)  & 0x1;
            uint64_t b10_5 = (inst >> 25) & 0x3F;
            uint64_t b4_1  = (inst >> 8)  & 0xF;
            
            uint64_t comb = (b12 << 12) | (b11 << 11) | (b10_5 << 5) | (b4_1 << 1);
            int64_t offset = sext(comb, 13);

            int64_t v1 = regs[((inst >> 15) & 0x1F)];
            int64_t v2 = regs[((inst >> 20) & 0x1F)];
            byte funct3 = (inst >> 12) & 0x7;
            bool take = false;

            if (funct3 == BEQ) take = (v1 == v2);
            else if (funct3 == BNE) take = (v1 != v2);
            else if (funct3 == BLT) take = (v1 < v2);
            else if (funct3 == BGE) take = (v1 >= v2);
            else if (funct3 == BLTU) take = ((uint64_t)v1 < (uint64_t)v2);
            else if (funct3 == BGEU) take = ((uint64_t)v1 >= (uint64_t)v2);

            if (take) pc = (pc - 1) + (offset / 4);
            
            DISPATCH();
        }

        exec_jal: {
            word inst = *pc++;
            uint64_t off20     = (inst >> 31) & 0x1;
            uint64_t off19_12 = (inst >> 12) & 0xFF;
            uint64_t off11     = (inst >> 20) & 0x1;
            uint64_t off10_1  = (inst >> 21) & 0x3FF;

            uint64_t comb = (off20 << 20) | (off19_12 << 12) | (off11 << 11) | (off10_1 << 1);
            int64_t offset = sext(comb, 21);

            pc = (pc - 1) + (offset / 4);
            DISPATCH();
        }

        exec_ecall: {
            pc++;
            int64_t service = regs[17];
            switch (service)
            {
                case EXIT: return; break;
                case IPRINT: std::cout << regs[10] << std::endl; break;
                case FPRINT: std::cout << fregs[10] << std::endl; break;
                case BPRINT: (regs[10]) ? std::cout << "true" << std::endl : std::cout << "false" << std::endl;
                default: break;
            }
            
            DISPATCH();
        }

        unknown_op: {
            std::cerr << "Opcode desconocido" << std::endl;
            return;
        }
    }
}