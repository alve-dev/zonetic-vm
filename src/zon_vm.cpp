#include "zon_vm.hpp"

#include <bit>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>

namespace zonvm {

// ------------------------------------------------------------------
// run() — threaded dispatch loop
//
// Uses GCC computed gotos (&&label) for a dispatch table instead of
// a switch, which avoids the branch misprediction cost of a central
// switch on every instruction.
//
// DISPATCH() jumps directly to the handler for the next instruction's
// opcode. Each handler ends with DISPATCH() to continue the loop.
// ------------------------------------------------------------------

void VM::run() {
    // setjmp lets ecall handlers do a non-local exit on fatal errors
    if (setjmp(execution_rescue_point) != 0) {
        free_ram(ram);
        return;
    }

    uint32_t* text_base = reinterpret_cast<uint32_t*>(ram);

    // sp must stay above this byte offset or we have a stack overflow
    const uint32_t STACK_GUARD = static_cast<uint32_t>(RAM_SIZE - STACK_LIMIT);

    // ------------------------------------------------------------------
    // Dispatch table — indexed by opcode (low 7 bits of instruction)
    // ------------------------------------------------------------------
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
    dispatch_table[OP_STR]    = &&exec_op_str;

    #define DISPATCH() goto *dispatch_table[*pc & 0x7F]

    DISPATCH();

    // ------------------------------------------------------------------
    // String operations (custom opcode 0x0B)
    // ------------------------------------------------------------------

    exec_op_str: {
        word inst   = *pc++;
        byte rd     = (inst >> 7)  & 0x1F;
        byte funct3 = (inst >> 12) & 0x7;
        byte rs1    = (inst >> 15) & 0x1F;
        byte rs2    = (inst >> 20) & 0x1F;
        byte funct7 = (inst >> 25) & 0x7F;

        if (funct7 == STANDARD_STR) {
            if (funct3 == CONCAT) {
                uint64_t ptr_a = regs[rs1];
                uint64_t ptr_b = regs[rs2];

                uint64_t len_a, len_b;
                std::memcpy(&len_a, &ram[ptr_a], 8);
                std::memcpy(&len_b, &ram[ptr_b], 8);

                uint64_t new_len    = len_a + len_b;
                uint64_t alloc_size = 8 + new_len + 1;
                uint64_t aligned    = (alloc_size + 7) & ~7ULL;

                uint32_t guard = static_cast<uint32_t>(RAM_SIZE - STACK_LIMIT);
                if (heap_bump + aligned >= guard) { regs[rd] = 0; DISPATCH(); }

                uint64_t dest = heap_bump;
                heap_bump += aligned;

                std::memcpy(&ram[dest],             &new_len,          8);
                std::memcpy(&ram[dest + 8],         &ram[ptr_a + 8],   len_a);
                std::memcpy(&ram[dest + 8 + len_a], &ram[ptr_b + 8],   len_b);
                ram[dest + 8 + new_len] = '\0';
                regs[rd] = dest;

            } else if (funct3 == EQ_STR) {
                uint64_t ptr_a = regs[rs1];
                uint64_t ptr_b = regs[rs2];

                uint64_t len_a, len_b;
                std::memcpy(&len_a, &ram[ptr_a], 8);
                std::memcpy(&len_b, &ram[ptr_b], 8);

                if (len_a != len_b) { regs[rd] = 0; DISPATCH(); }

                regs[rd] = (std::memcmp(&ram[ptr_a + 8], &ram[ptr_b + 8], len_a) == 0) ? 1 : 0;
            }
        }
        DISPATCH();
    }

    // ------------------------------------------------------------------
    // Integer immediate (OP-IMM, OP-IMM-32)
    // ------------------------------------------------------------------

    exec_op_imm: {
        word    inst   = *pc++;
        byte    rd     = (inst >> 7)  & 0x1F;
        byte    funct3 = (inst >> 12) & 0x7;
        int64_t v1     = regs[(inst >> 15) & 0x1F];
        int64_t imm    = sext((inst >> 20) & 0xFFF, 12);

        if      (funct3 == ADD_SUB)          regs[rd] = v1 + imm;
        else if (funct3 == SLL_SLLI)         regs[rd] = v1 << (imm & 0x3F);
        else if (funct3 == SLT_SLTI)         regs[rd] = (v1 < imm) ? 1 : 0;
        else if (funct3 == SLTU_SLTIU)       regs[rd] = (static_cast<uint64_t>(v1) < static_cast<uint64_t>(imm)) ? 1 : 0;
        else if (funct3 == XOR_XORI)         regs[rd] = v1 ^ imm;
        else if (funct3 == OR_ORI)           regs[rd] = v1 | imm;
        else if (funct3 == AND_ANDI)         regs[rd] = v1 & imm;
        else if (funct3 == SRL_SRA_SRLI_SRAI) {
            uint64_t shamt = static_cast<uint64_t>(imm) & 0x3F;
            bool arith     = (inst >> 30) & 1;
            regs[rd]       = arith ? (static_cast<int64_t>(v1) >> shamt)
                                   : (static_cast<uint64_t>(v1) >> shamt);
        }

        // stack overflow check — fires when sp (x2) moves below the guard
        if (rd == 2 && static_cast<uint64_t>(regs[2]) < STACK_GUARD) {
            std::cout << "\n[zon error]: Stack Overflow [ x_x] <(\"You exceeded the 128KB stack limit\")\n";
            free_ram(ram);
            std::exit(1);
        }

        regs[0] = 0;
        DISPATCH();
    }

    exec_op_imm_32: {
        word    inst   = *pc++;
        byte    rd     = (inst >> 7)  & 0x1F;
        byte    funct3 = (inst >> 12) & 0x7;
        int32_t v1     = static_cast<int32_t>(regs[(inst >> 15) & 0x1F]);
        int32_t imm    = static_cast<int32_t>(sext((inst >> 20) & 0xFFF, 12));

        if (funct3 == ADD_SUB) regs[rd] = static_cast<int64_t>(v1 + imm);

        regs[0] = 0;
        DISPATCH();
    }

    // ------------------------------------------------------------------
    // Integer register-register (OP, OP-32)
    // ------------------------------------------------------------------

    exec_op: {
        word    inst   = *pc++;
        byte    rd     = (inst >> 7)  & 0x1F;
        byte    funct3 = (inst >> 12) & 0x7;
        int64_t v1     = regs[(inst >> 15) & 0x1F];
        int64_t v2     = regs[(inst >> 20) & 0x1F];
        byte    funct7 = (inst >> 25) & 0x7F;

        if (funct7 == M_EXT_OR_FADD_D) {
            if      (funct3 == MUL) regs[rd] = v1 * v2;
            else if (funct3 == DIV) regs[rd] = (v2 != 0) ? v1 / v2 : 0;
            else if (funct3 == REM) regs[rd] = (v2 != 0) ? v1 % v2 : 0;

        } else if (funct7 == STANDARD) {
            if      (funct3 == ADD_SUB)           regs[rd] = v1 + v2;
            else if (funct3 == SLL_SLLI)          regs[rd] = v1 << (v2 & 0x3F);
            else if (funct3 == SLT_SLTI)          regs[rd] = (v1 < v2) ? 1 : 0;
            else if (funct3 == SLTU_SLTIU)        regs[rd] = (static_cast<uint64_t>(v1) < static_cast<uint64_t>(v2)) ? 1 : 0;
            else if (funct3 == XOR_XORI)          regs[rd] = v1 ^ v2;
            else if (funct3 == OR_ORI)            regs[rd] = v1 | v2;
            else if (funct3 == AND_ANDI)          regs[rd] = v1 & v2;
            else if (funct3 == SRL_SRA_SRLI_SRAI) regs[rd] = static_cast<uint64_t>(v1) >> (v2 & 0x3F);

        } else if (funct7 == ALT) {
            if      (funct3 == ADD_SUB)           regs[rd] = v1 - v2;
            else if (funct3 == SRL_SRA_SRLI_SRAI) regs[rd] = v1 >> (v2 & 0x3F);
            else if (funct3 == AND_ANDI)          regs[rd] = ~(v1 & v2);  // NAND
            else if (funct3 == OR_ORI)            regs[rd] = ~(v1 | v2);  // NOR
            else if (funct3 == XOR_XORI)          regs[rd] = ~(v1 ^ v2);  // XNOR
        }

        regs[0] = 0;
        DISPATCH();
    }

    exec_op_32: {
        word    inst   = *pc++;
        byte    rd     = (inst >> 7)  & 0x1F;
        byte    funct3 = (inst >> 12) & 0x7;
        int32_t v1     = static_cast<int32_t>(regs[(inst >> 15) & 0x1F]);
        int32_t v2     = static_cast<int32_t>(regs[(inst >> 20) & 0x1F]);
        byte    funct7 = (inst >> 25) & 0x7F;

        if (funct7 == M_EXT_OR_FADD_D) {
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

    // ------------------------------------------------------------------
    // Upper immediate (LUI, AUIPC)
    // ------------------------------------------------------------------

    exec_lui: {
        word inst = *pc++;
        byte rd   = (inst >> 7) & 0x1F;
        regs[rd]  = static_cast<int64_t>(static_cast<int32_t>(inst & 0xFFFFF000));
        regs[0]   = 0;
        DISPATCH();
    }

    exec_auipc: {
        word      inst   = *pc++;
        byte      rd     = (inst >> 7) & 0x1F;
        int32_t   imm    = static_cast<int32_t>(inst & 0xFFFFF000);
        uintptr_t pc_off = reinterpret_cast<uintptr_t>(pc - 1)
                         - reinterpret_cast<uintptr_t>(ram);
        regs[rd] = static_cast<int64_t>(pc_off) + imm;
        DISPATCH();
    }

    // ------------------------------------------------------------------
    // Loads and stores
    // ------------------------------------------------------------------

    exec_l: {
        word    inst   = *pc++;
        byte    rd     = (inst >> 7)  & 0x1F;
        byte    funct3 = (inst >> 12) & 0x7;
        int32_t imm    = static_cast<int32_t>(inst) >> 20;

        if (funct3 == LD) {
            uintptr_t addr = static_cast<uintptr_t>(regs[(inst >> 15) & 0x1F]) + imm;
            int64_t   val;
            std::memcpy(&val, &ram[addr], sizeof(int64_t));
            regs[rd] = val;
        }

        regs[0] = 0;
        DISPATCH();
    }

    exec_s: {
        word    inst     = *pc++;
        byte    funct3   = (inst >> 12) & 0x7;
        byte    rs1      = (inst >> 15) & 0x1F;
        byte    rs2      = (inst >> 20) & 0x1F;
        int32_t imm_11_5 = (inst >> 25) & 0x7F;
        int32_t imm_4_0  = (inst >> 7)  & 0x1F;
        int32_t imm      = (imm_11_5 << 5) | imm_4_0;
        if (imm & 0x800) imm |= 0xFFFFF000;

        if (funct3 == SD) {
            uintptr_t addr = static_cast<uintptr_t>(regs[rs1]) + imm;
            int64_t   val  = regs[rs2];
            std::memcpy(&ram[addr], &val, sizeof(int64_t));
        }

        DISPATCH();
    }

    exec_fl: {
        word    inst   = *pc++;
        byte    rd     = (inst >> 7)  & 0x1F;
        byte    funct3 = (inst >> 12) & 0x7;
        int32_t imm    = static_cast<int32_t>(inst) >> 20;

        if (funct3 == FLD) {
            uintptr_t addr = static_cast<uintptr_t>(regs[(inst >> 15) & 0x1F]) + imm;
            double    val;
            std::memcpy(&val, &ram[addr], sizeof(double));
            fregs[rd] = val;
        }

        DISPATCH();
    }

    exec_fs: {
        word    inst     = *pc++;
        byte    funct3   = (inst >> 12) & 0x7;
        byte    rs1      = (inst >> 15) & 0x1F;
        byte    rs2      = (inst >> 20) & 0x1F;
        int32_t imm_11_5 = static_cast<int32_t>(inst) >> 25;
        int32_t imm_4_0  = (inst >> 7) & 0x1F;
        int32_t imm      = (imm_11_5 << 5) | imm_4_0;

        if (funct3 == FSD) {
            uintptr_t addr = static_cast<uintptr_t>(regs[rs1]) + imm;
            double    val  = fregs[rs2];
            std::memcpy(&ram[addr], &val, sizeof(double));
        }

        DISPATCH();
    }

    // ------------------------------------------------------------------
    // Floating-point operations (OP-F)
    // ------------------------------------------------------------------

    exec_op_f: {
        word  inst   = *pc++;
        byte  rd     = (inst >> 7)  & 0x1F;
        byte  rm     = (inst >> 12) & 0x07;
        byte  rs1    = (inst >> 15) & 0x1F;
        byte  rs2    = (inst >> 20) & 0x1F;
        byte  funct7 = (inst >> 25) & 0x7F;

        if      (funct7 == FMV_D_X)  fregs[rd] = std::bit_cast<double>(regs[rs1]);
        else if (funct7 == FCVT_S_W) fregs[rd] = static_cast<double>(static_cast<float>(static_cast<int32_t>(regs[rs1])));
        else if (funct7 == FCVT_D_L) fregs[rd] = static_cast<double>(regs[rs1]);
        else if (funct7 == FMV_W_X)  fregs[rd] = static_cast<double>(std::bit_cast<float>(static_cast<uint32_t>(regs[rs1])));

        else if (funct7 == FSGNJ_S) {
            uint32_t b1 = std::bit_cast<uint32_t>(static_cast<float>(fregs[rs1]));
            uint32_t b2 = std::bit_cast<uint32_t>(static_cast<float>(fregs[rs2]));
            fregs[rd] = static_cast<double>(std::bit_cast<float>(perform_sign_injection<float, uint32_t>(b1, b2, rm)));
        }
        else if (funct7 == FSGNJ_D) {
            uint64_t b1 = std::bit_cast<uint64_t>(fregs[rs1]);
            uint64_t b2 = std::bit_cast<uint64_t>(fregs[rs2]);
            fregs[rd] = std::bit_cast<double>(perform_sign_injection<double, uint64_t>(b1, b2, rm));
        }

        // float32 arithmetic (results promoted to double internally)
        else if (funct7 == STANDARD) fregs[rd] = static_cast<double>(static_cast<float>(fregs[rs1]) + static_cast<float>(fregs[rs2]));
        else if (funct7 == FSUB_S)   fregs[rd] = static_cast<double>(static_cast<float>(fregs[rs1]) - static_cast<float>(fregs[rs2]));
        else if (funct7 == FMUL_S)   fregs[rd] = static_cast<double>(static_cast<float>(fregs[rs1]) * static_cast<float>(fregs[rs2]));
        else if (funct7 == FDIV_S) {
            float f2 = static_cast<float>(fregs[rs2]);
            fregs[rd] = (std::abs(f2) < std::numeric_limits<float>::epsilon())
                      ? 0.0 : static_cast<double>(static_cast<float>(fregs[rs1]) / f2);
        }

        // float64 arithmetic
        else if (funct7 == M_EXT_OR_FADD_D) fregs[rd] = fregs[rs1] + fregs[rs2];
        else if (funct7 == FSUB_D)           fregs[rd] = fregs[rs1] - fregs[rs2];
        else if (funct7 == FMUL_D)           fregs[rd] = fregs[rs1] * fregs[rs2];
        else if (funct7 == FDIV_D) {
            double f2 = fregs[rs2];
            fregs[rd] = (std::abs(f2) < std::numeric_limits<double>::epsilon())
                      ? 0.0 : fregs[rs1] / f2;
        }

        // comparisons (result goes to integer register)
        else if (funct7 == FCOMP_S) {
            float f1 = static_cast<float>(fregs[rs1]);
            float f2 = static_cast<float>(fregs[rs2]);
            if      (rm == 0x00) regs[rd] = (f1 <= f2) ? 1 : 0;
            else if (rm == 0x01) regs[rd] = (f1 <  f2) ? 1 : 0;
            else if (rm == 0x02) regs[rd] = (f1 == f2) ? 1 : 0;
        }
        else if (funct7 == FCOMP_D) {
            if      (rm == 0x00) regs[rd] = (fregs[rs1] <= fregs[rs2]) ? 1 : 0;
            else if (rm == 0x01) regs[rd] = (fregs[rs1] <  fregs[rs2]) ? 1 : 0;
            else if (rm == 0x02) regs[rd] = (fregs[rs1] == fregs[rs2]) ? 1 : 0;
        }

        DISPATCH();
    }

    // ------------------------------------------------------------------
    // Branches
    // ------------------------------------------------------------------

    exec_op_b: {
        word     inst   = *pc++;
        uint64_t b12    = (inst >> 31) & 0x1;
        uint64_t b11    = (inst >> 7)  & 0x1;
        uint64_t b10_5  = (inst >> 25) & 0x3F;
        uint64_t b4_1   = (inst >> 8)  & 0xF;
        int64_t  offset = sext((b12 << 12) | (b11 << 11) | (b10_5 << 5) | (b4_1 << 1), 13);

        int64_t v1     = regs[(inst >> 15) & 0x1F];
        int64_t v2     = regs[(inst >> 20) & 0x1F];
        byte    funct3 = (inst >> 12) & 0x7;
        bool    take   = false;

        if      (funct3 == BEQ)  take = (v1 == v2);
        else if (funct3 == BNE)  take = (v1 != v2);
        else if (funct3 == BLT)  take = (v1 <  v2);
        else if (funct3 == BGE)  take = (v1 >= v2);
        else if (funct3 == BLTU) take = (static_cast<uint64_t>(v1) <  static_cast<uint64_t>(v2));
        else if (funct3 == BGEU) take = (static_cast<uint64_t>(v1) >= static_cast<uint64_t>(v2));

        if (take) pc = (pc - 1) + (offset / 4);
        DISPATCH();
    }

    // ------------------------------------------------------------------
    // Jumps
    // ------------------------------------------------------------------

    exec_jal: {
        word     inst    = *pc++;
        byte     rd      = (inst >> 7) & 0x1F;
        uint64_t off20   = (inst >> 31) & 0x1;
        uint64_t off19_12 = (inst >> 12) & 0xFF;
        uint64_t off11   = (inst >> 20) & 0x1;
        uint64_t off10_1 = (inst >> 21) & 0x3FF;
        int64_t  offset  = sext((off20 << 20) | (off19_12 << 12) | (off11 << 11) | (off10_1 << 1), 21);

        if (rd != 0) regs[rd] = (pc - text_base) * 4;
        pc = (pc - 1) + (offset / 4);
        DISPATCH();
    }

    exec_jalr: {
        word    inst = *pc++;
        byte    rd   = (inst >> 7) & 0x1F;
        int64_t v1   = regs[(inst >> 15) & 0x1F];
        int64_t imm  = sext((inst >> 20) & 0xFFF, 12);

        if (rd != 0) regs[rd] = (pc - text_base) * 4;
        pc = text_base + (v1 + imm) / 4;
        DISPATCH();
    }

    // ------------------------------------------------------------------
    // Ecall — system interface
    // ------------------------------------------------------------------

    exec_ecall: {
        pc++;
        switch (static_cast<int64_t>(regs[17])) {

            case EXIT: {
                uint8_t code = static_cast<uint8_t>(regs[10]);
                free_ram(ram);
                std::exit(code);
            }

            // -- print --
            case IPRINT: std::printf("%ld",   regs[10]);  break;
            case FPRINT: std::printf("%.15g", fregs[10]); break;
            case BPRINT: std::printf("%s",    regs[10] ? "true" : "false"); break;
            case EPRINT: std::printf("\n"); break;
            case SPRINT: {
                uint64_t    ptr = regs[10];
                uint64_t    len;
                std::memcpy(&len, &ram[ptr], sizeof(uint64_t));
                const char* data = reinterpret_cast<const char*>(&ram[ptr + 8]);
                std::fwrite(data, 1, len, stdout);
                break;
            }

            // -- stack manage --
            case FILL_ZERO_STACK: {
                uint64_t vm_memory_address = regs[10];
                uint64_t total_bytes = regs[11];
                
                if (vm_memory_address + total_bytes <= RAM_SIZE) {
                    uint8_t* real_memory_ptr = ram + vm_memory_address;
                    std::memset(real_memory_ptr, 0, total_bytes);

                } else {
                    // TODO: -> ERROR SegFault / Out of bounds memory access in VM
                }
                break;
            }


            // -- heap arena --
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
                uint64_t size  = (static_cast<uint64_t>(regs[10]) + 7) & ~7ULL;
                uint32_t guard = static_cast<uint32_t>(RAM_SIZE - STACK_LIMIT);
                if (heap_bump + size >= guard) {
                    std::cerr << "[zon error]: Out of memory — heap collides with stack.\n";
                    free_ram(ram);
                    std::exit(1);
                }
                regs[10]   = heap_bump;
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

            // -- error message in runtime --
            case OUT_BOUND_INDEX_ERR: {
                uint32_t* code_start = reinterpret_cast<uint32_t*>(ram);
                uint32_t bytecode_pc = ((pc - 1) - code_start) * 4; 

                std::cerr << "error[E6001]: Index out of bounds!\n";
                std::cerr << "  --> runtime_panic at bytecode offset: " << bytecode_pc << "\n";
                std::cerr << "   |\n";
                std::cerr << "   | Instruction PC: " << bytecode_pc << " (check your .zonasm output at this address)\n";
                std::cerr << "   |\n";
                std::cerr << "  = note: In Zonetic, arrays are strictly bounds-checked. Accessing an\n";
                std::cerr << "          index outside [0, SIZE - 1] is illegal to guarantee memory safety.\n\n";
                
                std::cerr << " [ o_0] <(\"Ouch! You're trying to step outside the safe boundaries of your array.\n";
                std::cerr << "           I checked the index at PC " << bytecode_pc << " and it's out of range.\n";
                std::cerr << "           Double check your loops or variables to make sure they stay within limits!\")\n\n";

                free_ram(ram);
                std::exit(1);
            }

            default:
                std::cerr << "[zon error]: Unknown ecall (" << regs[17] << ")\n";
                free_ram(ram);
                std::exit(1);
        }
        DISPATCH();
    }

    // ------------------------------------------------------------------
    // Unknown opcode
    // ------------------------------------------------------------------

    unknown_op: {
        std::cerr << "[zon error]: Unknown opcode encountered.\n";
        free_ram(ram);
        return;
    }

    #undef DISPATCH
}

} // namespace zonvm