#include "zon_vm.hpp"
#include <iostream>
#include <bit>
#include <cstring>
#include <cmath>
#include <string>
#include <limits>

namespace zonvm {

    void VM::run() {
        if (setjmp(execution_rescue_point) != 0) {
            free_ram(ram);
            return;
        }

        uint32_t* text_base = reinterpret_cast<uint32_t*>(ram);

        const uint32_t STACK_GUARD = static_cast<uint32_t>(RAM_SIZE - STACK_LIMIT);

        static void* dispatch_table[128] = { &&unknown_op };
        dispatch_table[OP_IMM]    = &&exec_op_imm;
        dispatch_table[OP_IMM_32] = &&exec_op_imm_32;
        dispatch_table[OP]        = &&exec_op;
        dispatch_table[OP_32]     = &&exec_op_32;
        dispatch_table[OP_B]      = &&exec_op_b;
        dispatch_table[JAL]       = &&exec_jal;
        dispatch_table[ECALL]     = &&exec_ecall;
        dispatch_table[LUI]       = &&exec_lui;
        dispatch_table[OP_F]      = &&exec_op_f;
        dispatch_table[FL]        = &&exec_fl;
        dispatch_table[AUIPC]     = &&exec_auipc;
        dispatch_table[OP_L]      = &&exec_l;
        dispatch_table[OP_S]      = &&exec_s;
        dispatch_table[OP_FS]     = &&exec_fs;
        dispatch_table[JALR]      = &&exec_jalr;
        dispatch_table[OP_STR]     = &&exec_op_str;
        //dispatch_table[OP_IMM_STR] = &&exec_op_str_imm;

        #define DISPATCH() goto *dispatch_table[*pc & 0x7F]

        DISPATCH();

        exec_op_str: {
            word inst = *pc++;
            byte rd   = (inst >> 7) & 0x1F;
            byte funct3 = (inst >> 12) & 0x7;
            byte rs1 = (inst >> 15) & 0x1F;
            byte rs2 = (inst >> 20) & 0x1F;
            byte funct7 = (inst >> 25) & 0x7F;
            
            if (funct7 == STANDARD_STR) {
                if (funct3 == CONCAT) {
                    uint64_t ptr_a = regs[rs1];
                    uint64_t ptr_b = regs[rs2];
                    
                    uint64_t len_a, len_b;
                    std::memcpy(&len_a, &ram[ptr_a], 8);
                    std::memcpy(&len_b, &ram[ptr_b], 8);
                    
                    uint64_t new_len = len_a + len_b;
                    uint64_t alloc_size = 8 + new_len + 1;
                    uint64_t aligned = (alloc_size + 7) & ~7;
                    
                    uint32_t stack_guard = static_cast<uint32_t>(RAM_SIZE - STACK_LIMIT);
                    if (heap_bump + aligned >= stack_guard) {
                        regs[rd] = 0;
                        DISPATCH();
                    }
                    
                    uint64_t dest_ptr = heap_bump;
                    heap_bump += aligned;
                    
                    std::memcpy(&ram[dest_ptr], &new_len, 8);
                    std::memcpy(&ram[dest_ptr + 8], &ram[ptr_a + 8], len_a);
                    std::memcpy(&ram[dest_ptr + 8 + len_a], &ram[ptr_b + 8], len_b);
                    ram[dest_ptr + 8 + new_len] = '\0';
                    
                    regs[rd] = dest_ptr;
                }
                else if (funct3 == EQ_STR) {
                    uint64_t ptr_a = regs[rs1];
                    uint64_t ptr_b = regs[rs2];
                    
                    uint64_t len_a, len_b;
                    std::memcpy(&len_a, &ram[ptr_a], 8);
                    std::memcpy(&len_b, &ram[ptr_b], 8);
                    
                    if (len_a != len_b) {
                        regs[rd] = 0;
                        DISPATCH();             
                    }
                    
                    int cmp = std::memcmp(&ram[ptr_a + 8], &ram[ptr_b + 8], len_a);
                    regs[rd] = (cmp == 0) ? 1 : 0;
                }
            }
            DISPATCH();
        }
        
        /*exec_op_str_imm: {
            word inst = *pc++;
            byte rd   = (inst >> 7) & 0x1F;
            byte funct3 = (inst >> 12) & 0x7;
            byte rs1 = (inst >> 15) & 0x1F;
            int64_t imm = sext((inst >> 20) & 0xFFF, 12);
            
            if (funct3 == F3_STR_LEN) {
                uint64_t ptr = regs[rs1];
                uint64_t len;
                std::memcpy(&len, &ram[ptr], 8);
                regs[rd] = static_cast<int64_t>(len) + imm;  // imm permite desplazamiento
            }
            else if (funct3 == F3_STR_INDEX) {
                uint64_t ptr = regs[rs1];
                uint64_t index = static_cast<uint64_t>(regs[rd]);  // o rs2? cuidado
                uint64_t len;
                std::memcpy(&len, &ram[ptr], 8);
                if (index < len) {
                    regs[rd] = static_cast<int64_t>(ram[ptr + 8 + index]);
                } else {
                    regs[rd] = 0;
                }
            }
            DISPATCH();
        }*/

        exec_op_imm_32: {
            word inst   = *pc++;
            byte rd     = (inst >> 7)  & 0x1F;
            byte funct3 = (inst >> 12) & 0x7;
            int32_t v1  = static_cast<int32_t>(regs[(inst >> 15) & 0x1F]);
            int32_t imm = static_cast<int32_t>(sext((inst >> 20) & 0xFFF, 12));
            if (funct3 == ADD_SUB) regs[rd] = static_cast<int64_t>(v1 + imm);
            regs[0] = 0;
            DISPATCH();
        }

        exec_op_32: {
            word inst   = *pc++;
            byte rd     = (inst >> 7)  & 0x1F;
            byte funct3 = (inst >> 12) & 0x7;
            int32_t v1  = static_cast<int32_t>(regs[(inst >> 15) & 0x1F]);
            int32_t v2  = static_cast<int32_t>(regs[(inst >> 20) & 0x1F]);
            byte funct7 = (inst >> 25) & 0x7F;
            if      (funct7 == M_EXT_OR_FADD_D) {
                if      (funct3 == MUL) regs[rd] = static_cast<int64_t>(v1 * v2);
                else if (funct3 == DIV) regs[rd] = (v2 != 0) ? static_cast<int64_t>(v1 / v2) : 0;
                else if (funct3 == REM) regs[rd] = (v2 != 0) ? static_cast<int64_t>(v1 % v2) : 0;
            } else if (funct7 == STANDARD) {
                if (funct3 == ADD_SUB) regs[rd] = static_cast<int64_t>(v1 + v2);
            } else if (funct7 == ALT) {
                if (funct3 == ADD_SUB) regs[rd] = static_cast<int64_t>(v1 - v2);
            }
            regs[0] = 0;
            DISPATCH();
        }

        exec_op_imm: {
            word inst   = *pc++;
            byte rd     = (inst >> 7)  & 0x1F;
            byte funct3 = (inst >> 12) & 0x7;
            int64_t v1  = regs[(inst >> 15) & 0x1F];
            int64_t imm = sext((inst >> 20) & 0xFFF, 12);

            if      (funct3 == ADD_SUB)    regs[rd] = v1 + imm;
            else if (funct3 == SLL_SLLI)   regs[rd] = v1 << (imm & 0x3F);
            else if (funct3 == SLT_SLTI)   regs[rd] = (v1 < imm) ? 1 : 0;
            else if (funct3 == SLTU_SLTIU) regs[rd] = (static_cast<uint64_t>(v1) < static_cast<uint64_t>(imm)) ? 1 : 0;
            else if (funct3 == XOR_XORI)   regs[rd] = v1 ^ imm;
            else if (funct3 == OR_ORI)     regs[rd] = v1 | imm;
            else if (funct3 == AND_ANDI)   regs[rd] = v1 & imm;
            else if (funct3 == SRL_SRA_SRLI_SRAI) {
                uint64_t shamt = static_cast<uint64_t>(imm) & 0x3F;
                bool arith = (inst >> 30) & 1;
                regs[rd] = arith ? (static_cast<int64_t>(v1) >> shamt)
                                 : (static_cast<uint64_t>(v1) >> shamt);
            }

            // Stack overflow check cuando sp cambia
            if (rd == 2 && static_cast<uint64_t>(regs[2]) < STACK_GUARD) {
                std::cout << "\n[zon error]: Stack Overflow, [X_X] <(\"You exceeded the 128KB stack limit\")\n";
                free_ram(ram);
                std::exit(1);
            }

            regs[0] = 0;
            DISPATCH();
        }

        exec_lui: {
            word inst = *pc++;
            byte rd   = (inst >> 7) & 0x1F;
            regs[rd]  = static_cast<int64_t>(static_cast<int32_t>(inst & 0xFFFFF000));
            regs[0]   = 0;
            DISPATCH();
        }

        exec_auipc: {
            word inst    = *pc++;
            byte rd      = (inst >> 7) & 0x1F;
            int32_t imm  = static_cast<int32_t>(inst & 0xFFFFF000);
            uintptr_t pc_byte_offset = reinterpret_cast<uintptr_t>(pc - 1)
                                     - reinterpret_cast<uintptr_t>(ram);
            regs[rd] = static_cast<int64_t>(pc_byte_offset) + imm;
            DISPATCH();
        }

        exec_op: {
            word inst   = *pc++;
            byte rd     = (inst >> 7)  & 0x1F;
            byte funct3 = (inst >> 12) & 0x7;
            int64_t v1  = regs[(inst >> 15) & 0x1F];
            int64_t v2  = regs[(inst >> 20) & 0x1F];
            byte funct7 = (inst >> 25) & 0x7F;

            if (funct7 == M_EXT_OR_FADD_D) {
                if      (funct3 == MUL) regs[rd] = v1 * v2;
                else if (funct3 == DIV) regs[rd] = (v2 != 0) ? v1 / v2 : 0;
                else if (funct3 == REM) regs[rd] = (v2 != 0) ? v1 % v2 : 0;
            } else if (funct7 == STANDARD) {
                if      (funct3 == ADD_SUB)          regs[rd] = v1 + v2;
                else if (funct3 == SLL_SLLI)         regs[rd] = v1 << (v2 & 0x3F);
                else if (funct3 == SLT_SLTI)         regs[rd] = (v1 < v2) ? 1 : 0;
                else if (funct3 == SLTU_SLTIU)       regs[rd] = (static_cast<uint64_t>(v1) < static_cast<uint64_t>(v2)) ? 1 : 0;
                else if (funct3 == XOR_XORI)         regs[rd] = v1 ^ v2;
                else if (funct3 == OR_ORI)           regs[rd] = v1 | v2;
                else if (funct3 == AND_ANDI)         regs[rd] = v1 & v2;
                else if (funct3 == SRL_SRA_SRLI_SRAI) regs[rd] = static_cast<uint64_t>(v1) >> (v2 & 0x3F);
            } else if (funct7 == ALT) {
                if      (funct3 == ADD_SUB)          regs[rd] = v1 - v2;
                else if (funct3 == SRL_SRA_SRLI_SRAI) regs[rd] = v1 >> (v2 & 0x3F);
            }

            regs[0] = 0;
            DISPATCH();
        }

        exec_l: {
            word inst   = *pc++;
            byte rd     = (inst >> 7)  & 0x1F;
            byte funct3 = (inst >> 12) & 0x7;
            int32_t imm = static_cast<int32_t>(inst) >> 20;

            if (funct3 == LD) {
                uintptr_t addr = static_cast<uintptr_t>(regs[(inst >> 15) & 0x1F]) + imm;
                int64_t val;
                std::memcpy(&val, &ram[addr], sizeof(int64_t));
                regs[rd] = val;
            }

            regs[0] = 0;
            DISPATCH();
        }

        exec_s: {
            word inst       = *pc++;
            byte funct3     = (inst >> 12) & 0x7;
            byte rs1        = (inst >> 15) & 0x1F;
            byte rs2        = (inst >> 20) & 0x1F;
            int32_t imm_11_5 = (inst >> 25) & 0x7F;
            int32_t imm_4_0  = (inst >> 7)  & 0x1F;
            int32_t imm      = (imm_11_5 << 5) | imm_4_0;
            if (imm & 0x800) imm |= 0xFFFFF000;

            if (funct3 == SD) {
                uintptr_t addr = static_cast<uintptr_t>(regs[rs1]) + imm;
                int64_t val = regs[rs2];
                std::memcpy(&ram[addr], &val, sizeof(int64_t));
            }

            DISPATCH();
        }

        exec_fl: {
            word inst   = *pc++;
            byte rd     = (inst >> 7)  & 0x1F;
            byte funct3 = (inst >> 12) & 0x7;
            int32_t imm = static_cast<int32_t>(inst) >> 20;

            if (funct3 == FLD) {
                uintptr_t addr = static_cast<uintptr_t>(regs[(inst >> 15) & 0x1F]) + imm;
                double val;
                std::memcpy(&val, &ram[addr], sizeof(double));
                fregs[rd] = val;
            }

            DISPATCH();
        }

        exec_fs: {
            word inst       = *pc++;
            byte funct3     = (inst >> 12) & 0x7;
            byte rs1        = (inst >> 15) & 0x1F;
            byte rs2        = (inst >> 20) & 0x1F;
            int32_t imm_11_5 = static_cast<int32_t>(inst) >> 25;
            int32_t imm_4_0  = (inst >> 7) & 0x1F;
            int32_t imm      = (imm_11_5 << 5) | imm_4_0;

            if (funct3 == FSD) {
                uintptr_t addr = static_cast<uintptr_t>(regs[rs1]) + imm;
                double val = fregs[rs2];
                std::memcpy(&ram[addr], &val, sizeof(double));
            }

            DISPATCH();
        }

        exec_op_f: {
            word inst   = *pc++;
            byte rd     = (inst >> 7)  & 0x1F;
            byte rm     = (inst >> 12) & 0x07;
            byte rs1    = (inst >> 15) & 0x1F;
            byte rs2    = (inst >> 20) & 0x1F;
            byte funct7 = (inst >> 25) & 0x7F;

            if      (funct7 == FMV_D_X)       fregs[rd] = std::bit_cast<double>(regs[rs1]);
            else if (funct7 == FCVT_S_W)      fregs[rd] = static_cast<double>(static_cast<float>(static_cast<int32_t>(regs[rs1])));
            else if (funct7 == FCVT_D_L)      fregs[rd] = static_cast<double>(regs[rs1]);
            else if (funct7 == FMV_W_X)       fregs[rd] = static_cast<double>(std::bit_cast<float>(static_cast<uint32_t>(regs[rs1])));
            else if (funct7 == FSGNJ_S) {
                uint32_t b1 = std::bit_cast<uint32_t>(static_cast<float>(fregs[rs1]));
                uint32_t b2 = std::bit_cast<uint32_t>(static_cast<float>(fregs[rs2]));
                fregs[rd] = static_cast<double>(std::bit_cast<float>(perform_sign_injection<float, uint32_t>(b1, b2, rm)));
            } else if (funct7 == FSGNJ_D) {
                uint64_t b1 = std::bit_cast<uint64_t>(fregs[rs1]);
                uint64_t b2 = std::bit_cast<uint64_t>(fregs[rs2]);
                fregs[rd] = std::bit_cast<double>(perform_sign_injection<double, uint64_t>(b1, b2, rm));
            } else if (funct7 == STANDARD) {
                fregs[rd] = static_cast<double>(static_cast<float>(fregs[rs1]) + static_cast<float>(fregs[rs2]));
            } else if (funct7 == M_EXT_OR_FADD_D) { fregs[rd] = fregs[rs1] + fregs[rs2];
            } else if (funct7 == FSUB_S) {
                fregs[rd] = static_cast<double>(static_cast<float>(fregs[rs1]) - static_cast<float>(fregs[rs2]));
            } else if (funct7 == FSUB_D)  { fregs[rd] = fregs[rs1] - fregs[rs2];
            } else if (funct7 == FMUL_S) {
                fregs[rd] = static_cast<double>(static_cast<float>(fregs[rs1]) * static_cast<float>(fregs[rs2]));
            } else if (funct7 == FMUL_D)  { fregs[rd] = fregs[rs1] * fregs[rs2];
            } else if (funct7 == FDIV_S) {
                float f2 = static_cast<float>(fregs[rs2]);
                fregs[rd] = (std::abs(f2) < std::numeric_limits<float>::epsilon())
                          ? 0.0 : static_cast<double>(static_cast<float>(fregs[rs1]) / f2);
            } else if (funct7 == FDIV_D) {
                double f2 = fregs[rs2];
                fregs[rd] = (std::abs(f2) < std::numeric_limits<double>::epsilon())
                          ? 0.0 : fregs[rs1] / f2;
            } else if (funct7 == FCOMP_S) {
                float f1 = static_cast<float>(fregs[rs1]);
                float f2 = static_cast<float>(fregs[rs2]);
                if      (rm == 0x00) regs[rd] = (f1 <= f2) ? 1 : 0;
                else if (rm == 0x01) regs[rd] = (f1 <  f2) ? 1 : 0;
                else if (rm == 0x02) regs[rd] = (f1 == f2) ? 1 : 0;
            } else if (funct7 == FCOMP_D) {
                if      (rm == 0x00) regs[rd] = (fregs[rs1] <= fregs[rs2]) ? 1 : 0;
                else if (rm == 0x01) regs[rd] = (fregs[rs1] <  fregs[rs2]) ? 1 : 0;
                else if (rm == 0x02) regs[rd] = (fregs[rs1] == fregs[rs2]) ? 1 : 0;
            }

            DISPATCH();
        }

        exec_op_b: {
            word inst    = *pc++;
            uint64_t b12  = (inst >> 31) & 0x1;
            uint64_t b11  = (inst >> 7)  & 0x1;
            uint64_t b105 = (inst >> 25) & 0x3F;
            uint64_t b41  = (inst >> 8)  & 0xF;
            int64_t offset = sext((b12 << 12) | (b11 << 11) | (b105 << 5) | (b41 << 1), 13);

            int64_t v1  = regs[(inst >> 15) & 0x1F];
            int64_t v2  = regs[(inst >> 20) & 0x1F];
            byte funct3 = (inst >> 12) & 0x7;
            bool take   = false;

            if      (funct3 == BEQ)  take = (v1 == v2);
            else if (funct3 == BNE)  take = (v1 != v2);
            else if (funct3 == BLT)  take = (v1 <  v2);
            else if (funct3 == BGE)  take = (v1 >= v2);
            else if (funct3 == BLTU) take = (static_cast<uint64_t>(v1) <  static_cast<uint64_t>(v2));
            else if (funct3 == BGEU) take = (static_cast<uint64_t>(v1) >= static_cast<uint64_t>(v2));

            if (take) pc = (pc - 1) + (offset / 4);
            DISPATCH();
        }

        exec_jal: {
            word inst     = *pc++;
            byte rd       = (inst >> 7) & 0x1F;
            uint64_t off20   = (inst >> 31) & 0x1;
            uint64_t off1912 = (inst >> 12) & 0xFF;
            uint64_t off11   = (inst >> 20) & 0x1;
            uint64_t off101  = (inst >> 21) & 0x3FF;
            int64_t offset   = sext((off20 << 20) | (off1912 << 12) | (off11 << 11) | (off101 << 1), 21);

            if (rd != 0) regs[rd] = pc - text_base;
            pc = (pc - 1) + (offset / 4);
            DISPATCH();
        }

        exec_jalr: {
            word inst  = *pc++;
            byte rd    = (inst >> 7) & 0x1F;
            int64_t v1 = regs[(inst >> 15) & 0x1F];
            int64_t imm = sext((inst >> 20) & 0xFFF, 12);

            if (rd != 0) regs[rd] = pc - text_base;
            pc = text_base + v1 + (imm / 4);
            DISPATCH();
        }

        exec_ecall: {
            pc++;
            switch (static_cast<int64_t>(regs[17])) {

                case EXIT: {
                    uint8_t code = static_cast<uint8_t>(regs[10]);
                    free_ram(ram);
                    std::exit(code);
                }
                case IPRINT: std::printf("%ld",   regs[10]); break;
                case FPRINT: std::printf("%.15g", fregs[10]); break;
                case BPRINT: std::printf("%s",    regs[10] ? "true" : "false"); break;
                case SPRINT: {
                    uint64_t ptr = regs[10];
                    uint64_t len;
                    std::memcpy(&len, &ram[ptr], sizeof(uint64_t));
                    const char* data = reinterpret_cast<const char*>(&ram[ptr + 8]);
                    std::fwrite(data, 1, len, stdout);
                    break;
                }
                case EPRINT: std::printf("\n"); break;

                case HEAP_PUSH: {
                    arena_stack.push_back(heap_bump);
                    break;
                }
                case HEAP_POP: {
                    if (!arena_stack.empty()) {
                        heap_bump = arena_stack.back();
                        arena_stack.pop_back();
                    }
                    break;
                }
                case HEAP_ALLOC: {
                    uint64_t size = (static_cast<uint64_t>(regs[10]) + 7) & ~7ULL;
                    uint32_t stack_guard = static_cast<uint32_t>(RAM_SIZE - STACK_LIMIT);
                    if (heap_bump + size >= stack_guard) {
                        std::cerr << "[zon error]: out of memory (heap colisiona con stack)\n";
                        free_ram(ram);
                        std::exit(1);
                    }
                    regs[10]  = heap_bump;
                    heap_bump += static_cast<uint32_t>(size);
                    break;
                }
                case HEAP_STORE: {
                    uintptr_t ptr = static_cast<uintptr_t>(regs[10]);
                    int64_t   val = regs[11];
                    std::memcpy(&ram[ptr], &val, 8);
                    break;
                }
                case HEAP_LOAD: {
                    uintptr_t ptr = static_cast<uintptr_t>(regs[10]);
                    int64_t   val;
                    std::memcpy(&val, &ram[ptr], 8);
                    regs[10] = val;
                    break;
                }

                default:
                    std::cerr << "[zon error]: unknown ecall (" << regs[17] << ")\n";
                    free_ram(ram);
                    std::exit(1);
            }
            DISPATCH();
        }

        unknown_op: {
            std::cerr << "[zon error]: opcode desconocido\n";
            free_ram(ram);
            return;
        }

        #undef DISPATCH
    }

} // namespace zonvm