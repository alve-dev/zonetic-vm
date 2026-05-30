#include "zon_vm.hpp"
#include <iostream>
#include <bit>
#include <cstring>
#include <cmath>
#include <string>
#include <csignal>
#include <csetjmp>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace zonvm {
    static std::jmp_buf execution_rescue_point;

    #ifdef _WIN32
        LONG WINAPI zonetic_windows_exception_handler(PEXCEPTION_POINTERS exception_info) {
            if (exception_info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
                std::cout << "\n[zon error]: Stack Overflow, [X_X] <(\"You exceeded the 128KB stack limit\")\n";
                std::longjmp(execution_rescue_point, 1);
            }
            
            return EXCEPTION_CONTINUE_SEARCH;
        }
    #else
        static void zonetic_stack_overflow_handler(int signum, siginfo_t* info, void* context) {
            std::cout << "\n[zon error]: Stack Overflow, [X_X] <(\"You exceeded the 128KB stack limit\")\n";
            std::longjmp(execution_rescue_point, 1);
        }
    #endif

    static uint8_t* allocate_protected_ram(size_t total_ram_size, size_t stack_limit_size) {
    #ifdef _WIN32
        uint8_t* raw_ram = reinterpret_cast<uint8_t*>(VirtualAlloc(
            nullptr, total_ram_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE
        ));

        if (!raw_ram) {
            std::cerr << "[zon error]: [zon error]: The virtual memory (Windows) could not be mapped..\n";
            std::exit(1);
        }

        SYSTEM_INFO si;
        GetSystemInfo(&si);
        size_t page_size = si.dwPageSize;

        size_t guard_offset = (total_ram_size - stack_limit_size) & ~(page_size - 1);
        uint8_t* guard_page_address = raw_ram + guard_offset;

        DWORD old_protect;
        VirtualProtect(guard_page_address, page_size, PAGE_NOACCESS, &old_protect);

        AddVectoredExceptionHandler(1, zonetic_windows_exception_handler);
        
        return raw_ram;

    #else
        size_t page_size = sysconf(_SC_PAGESIZE);
        uint8_t* raw_ram = reinterpret_cast<uint8_t*>(mmap(
            nullptr, total_ram_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0
        ));

        if (raw_ram == MAP_FAILED) {
            std::cerr << "[zon error]: The virtual memory (mmap) could not be mapped.\n";
            std::exit(1);
        }

        size_t guard_offset = (total_ram_size - stack_limit_size) & ~(page_size - 1);
        uint8_t* guard_page_address = raw_ram + guard_offset;

        if (mprotect(guard_page_address, page_size, PROT_NONE) == -1) {
            std::cerr << "[zon error]: Failed to pin mprotect to the Stack\n";
            std::exit(1);
        }
        
        struct sigaction sa;
        sa.sa_sigaction = zonetic_stack_overflow_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_SIGINFO;
        sigaction(SIGSEGV, &sa, nullptr);

        return raw_ram;
    #endif
    }

    void free_protected_ram(uint8_t* ram_ptr, size_t total_size) {
    #ifdef _WIN32
        VirtualFree(ram_ptr, 0, MEM_RELEASE);
    #else
        munmap(ram_ptr, total_size);
    #endif
    }

    void VM::run() {
        pc = code.data();
        word* end = pc + code.size();

        const size_t RAM_SIZE = 1024 * 1024;
        const size_t STACK_LIMIT = 1024 * 128;
        
        uint8_t* protected_ram = allocate_protected_ram(RAM_SIZE, STACK_LIMIT);

        regs[2] = static_cast<int64_t>(RAM_SIZE - 16);

        if (setjmp(execution_rescue_point) != 0) {
            free_protected_ram(protected_ram, RAM_SIZE);
            return; 
        }

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

        #define DISPATCH() \
            goto *dispatch_table[*pc & 0x7F]
        
        DISPATCH();

        exec_op_imm_32: {
            word inst = *pc++;
            byte rd = (inst >> 7) & 0x1F;
            byte funct3 = (inst >> 12) & 0x7;
            int32_t v1 = (int32_t)regs[((inst >> 15) & 0x1F)];
            int32_t immI = (int32_t)sext(((inst >> 20) & 0xFFF), 12);

            if (funct3 == ADD_SUB) {
                regs[rd] = (int64_t)(v1 + immI); 
            }
            
            regs[0] = 0;
            DISPATCH();
        }

        exec_op_32: {
            word inst = *pc++;
            byte rd     = (inst >> 7) & 0x1F;
            byte funct3 = (inst >> 12) & 0x7;
            int32_t v1  = (int32_t)regs[((inst >> 15) & 0x1F)];
            int32_t v2  = (int32_t)regs[((inst >> 20) & 0x1F)];
            byte funct7 = (inst >> 25) & 0x7F;

            if (funct7 == M_EXT_OR_FADD_D) {
                if (funct3 == MUL)      regs[rd] = (int64_t)(v1 * v2);
                else if (funct3 == DIV) regs[rd] = (v2 != 0) ? (int64_t)(v1 / v2) : 0;
                else if (funct3 == REM) regs[rd] = (v2 != 0) ? (int64_t)(v1 % v2) : 0;
            } else if (funct7 == STANDARD) {
                if (funct3 == ADD_SUB)  regs[rd] = (int64_t)(v1 + v2);
            } else if (funct7 == ALT) {
                if (funct3 == ADD_SUB)  regs[rd] = (int64_t)(v1 - v2);
            }

            regs[0] = 0;
            DISPATCH();
        }

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

        exec_auipc: {
            word inst = *pc++;
            byte rd = (inst >> 7) & 0x1F;
            int32_t imm = (int32_t)(inst & 0xFFFFF000);
            uintptr_t relative_pc = (reinterpret_cast<uintptr_t>(pc - 1) - reinterpret_cast<uintptr_t>(code.data()));
            regs[rd] = relative_pc + imm; 
            DISPATCH();
        }

        exec_op: {
            word inst = *pc++;
            byte rd      = (inst >> 7) & 0x1F;
            byte funct3  = (inst >> 12) & 0x7;
            int64_t v1   = regs[((inst >> 15) & 0x1F)];
            int64_t v2   = regs[((inst >> 20) & 0x1F)];
            byte funct7  = (inst >> 25) & 0x7F;

            if (funct7 == M_EXT_OR_FADD_D) {
                if (funct3 == MUL)      regs[rd] = v1 * v2;
                else if (funct3 == DIV) regs[rd] = (v2 != 0) ? v1 / v2 : 0;
                else if (funct3 == REM) regs[rd] = (v2 != 0) ? v1 % v2 : 0;
            } else if (funct7 == STANDARD) {
                if (funct3 == ADD_SUB)      regs[rd] = v1 + v2;
                else if (funct3 == SLT_SLTI)  regs[rd] = (v1 < v2) ? 1 : 0;
                else if (funct3 == SLTU_SLTIU) regs[rd] = ((uint64_t)v1 < (uint64_t)v2) ? 1 : 0;
                else if (funct3 == XOR_XORI)   regs[rd] = v1 ^ v2;
                else if (funct3 == OR_ORI)     regs[rd] = v1 | v2;
                else if (funct3 == AND_ANDI)   regs[rd] = v1 & v2;
            } else if (funct7 == ALT) {
                if (funct3 == ADD_SUB)      regs[rd] = v1 - v2;
            }

            regs[0] = 0;
            DISPATCH();
        }

        exec_l: {
            word inst = *pc++;
            byte rd     = (inst >> 7) & 0x1F;
            byte funct3 = (inst >> 12) & 0x7;
            byte rs1    = (inst >> 15) & 0x1F;
            int32_t imm     = static_cast<int32_t>(inst) >> 20;
            
            if (funct3 == LD) {
                uintptr_t final_address = regs[rs1] + imm;
                int64_t val;
                if (rs1 == 2 || rs1 == 8) {
                    std::memcpy(&val, &protected_ram[final_address], sizeof(int64_t));
                    regs[rd] = val;

                } else if (rs1 == 3) {
                    std::memcpy(&val, &data[final_address], sizeof(int64_t));
                    regs[rd] = val; 
                    
                } else {
                    uintptr_t pool_index = final_address - (code.size() * 4);
                    if (pool_index + 8 <= pool_data.size()) {
                        std::memcpy(&val, &pool_data[pool_index], sizeof(int64_t));
                        regs[rd] = val;
                    }
                }
            }

            regs[0] = 0;
            DISPATCH();
        }

        exec_s : {
            word inst = *pc++;
            byte funct3 = (inst >> 12) & 0x7;
            byte rs1 = (inst >> 15) & 0x1F;
            byte rs2 = (inst >> 20) & 0x1F;

            int32_t imm_11_5 = (inst >> 25) & 0x7F;
            int32_t imm_4_0  = (inst >> 7) & 0x1F;
            int32_t imm = (imm_11_5 << 5) | imm_4_0;

            if (imm & 0x800) {
                imm |= 0xFFFFF000; 
            }

            if (funct3 == SD) {
                uintptr_t final_address = regs[rs1] + imm;

                if (rs1 == 2 || rs1 == 8) { 
                    int64_t val = regs[rs2];
                    std::memcpy(&protected_ram[final_address], &val, sizeof(int64_t));
                    
                } else if (rs1 == 3) {
                    int64_t val = regs[rs2];
                    std::memcpy(&data[final_address], &val, sizeof(int64_t));
                    
                } else {
                    uintptr_t pool_index = final_address - (code.size() * 4);
                    if (pool_index + 8 <= pool_data.size()) {
                        int64_t val = regs[rs2];
                        std::memcpy(&pool_data[pool_index], &val, sizeof(int64_t));
                    }
                }
            }

            DISPATCH();
        }

        exec_fs : {
            word inst = *pc++;
            byte funct3 = (inst >> 12) & 0x7;
            byte rs1 = (inst >> 15) & 0x1F;
            byte rs2 = (inst >> 20) & 0x1F;

            int32_t imm_11_5 = (inst >> 25) & 0x7F;
            int32_t imm_4_0  = (inst >> 7) & 0x1F;
            int32_t imm = (imm_11_5 << 5) | imm_4_0;

            if (imm & 0x800) {
                imm |= 0xFFFFF000; 
            }

            if (funct3 == FSD) {
                uintptr_t final_address = regs[rs1] + imm;

                if (rs1 == 2 || rs1 == 8) {
                    double val = fregs[rs2];
                    std::memcpy(&protected_ram[final_address], &val, sizeof(double));
                    
                } else if (rs1 == 3) {
                    if (final_address + 8 <= data.size()) {
                        double val = fregs[rs2];
                        std::memcpy(&data[final_address], &val, sizeof(double));
                    }
                } else {
                    uintptr_t pool_index = final_address - (code.size() * 4);
                    double val = fregs[rs2];
                    if (pool_index + 8 <= pool_data.size()) {
                        std::memcpy(&pool_data[pool_index], &val, sizeof(double));
                    }
                }
            }

            DISPATCH();
        }

        exec_fl: {
            word inst = *pc++;
            byte rd     = (inst >> 7) & 0x1F;
            uint32_t funct3 = (inst >> 12) & 0x7;
            byte rs1    = (inst >> 15) & 0x1F;
            
            int32_t imm = (inst >> 20) & 0xFFF;
            if (imm & 0x800) imm |= 0xFFFFF000;

            if (funct3 == FLD) {
                uintptr_t final_offset = regs[rs1] + imm;
                double val;

                if (rs1 == 2 || rs1 == 8) {
                    std::memcpy(&val, &protected_ram[final_offset], sizeof(double));
                    fregs[rd] = val;
                    
                } else if (rs1 == 3) {
                    if (final_offset + 8 <= data.size()) {
                        std::memcpy(&val, &data[final_offset], sizeof(double));
                        fregs[rd] = val; 
                    }
                } else { 
                    uintptr_t pool_index = final_offset - (code.size() * 4);
                    if (pool_index + 8 <= pool_data.size()) {
                        std::memcpy(&val, &pool_data[pool_index], sizeof(double));
                        fregs[rd] = val;
                    }
                }
            }
            DISPATCH();
        }

        exec_op_f: {
            word inst = *pc++;
            byte rd     = (inst >> 7)  & 0x1F;
            byte rm     = (inst >> 12) & 0x07;
            byte rs1    = (inst >> 15) & 0x1F;
            byte rs2    = (inst >> 20) & 0x1F;
            byte funct7 = (inst >> 25) & 0x7F;
            
            if (funct7 == FMV_D_X) {
                fregs[rd] = std::bit_cast<double>(regs[rs1]);
            }
            else if (funct7 == FCVT_S_W) {
                float f = static_cast<float>((int32_t)regs[rs1]);
                fregs[rd] = (double)f;
            } else if (funct7 == FSGNJ_S) {
                uint32_t b1 = std::bit_cast<uint32_t>((float)fregs[rs1]);
                uint32_t b2 = std::bit_cast<uint32_t>((float)fregs[rs2]);
                fregs[rd] = (double)std::bit_cast<float>(perform_sign_injection<float, uint32_t>(b1, b2, rm));
            } else if (funct7 == FSGNJ_D) {
                uint64_t b1 = std::bit_cast<uint64_t>(fregs[rs1]);
                uint64_t b2 = std::bit_cast<uint64_t>(fregs[rs2]);
                fregs[rd] = std::bit_cast<double>(perform_sign_injection<double, uint64_t>(b1, b2, rm));
            } else if (funct7 == FMV_W_X) {
                float f = std::bit_cast<float>((uint32_t)regs[rs1]);
                fregs[rd] = (double)f;
            } else if (funct7 == STANDARD) {
                float f1 = (float)fregs[rs1];
                float f2 = (float)fregs[rs2];
                fregs[rd] = (double)(f1 + f2);
            } else if (funct7 == M_EXT_OR_FADD_D) {
                fregs[rd] = fregs[rs1] + fregs[rs2];
            } else if (funct7 == FSUB_S) {
                float f1 = (float)fregs[rs1];
                float f2 = (float)fregs[rs2];
                fregs[rd] = (double)(f1 - f2);
            } else if (funct7 == FSUB_D) {
                fregs[rd] = fregs[rs1] - fregs[rs2];
            } else if (funct7 == FMUL_S) {
                float f1 = (float)fregs[rs1];
                float f2 = (float)fregs[rs2];
                fregs[rd] = (double)(f1 * f2);
            } else if (funct7 == FMUL_D) {
                fregs[rd] = fregs[rs1] * fregs[rs2];
            } else if (funct7 == FDIV_S) {
                float f1 = (float)fregs[rs1];
                float f2 = (float)fregs[rs2];
                if (std::abs(f2) < std::numeric_limits<float>::epsilon()) fregs[rd] = 0.0;
                else fregs[rd] = (double)(f1 / f2);
            } else if (funct7 == FDIV_D) {
                double f2 = fregs[rs2];
                if (std::abs(f2) < std::numeric_limits<double>::epsilon()) fregs[rd] = 0.0;
                else fregs[rd] = fregs[rs1] / f2;
            } else if (funct7 == FCOMP_S) {
                float f1 = (float)fregs[rs1];
                float f2 = (float)fregs[rs2];
                int64_t res = 0;
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
            byte rd = (inst >> 7) & 0x1F;
            uint64_t off20     = (inst >> 31) & 0x1;
            uint64_t off19_12 = (inst >> 12) & 0xFF;
            uint64_t off11     = (inst >> 20) & 0x1;
            uint64_t off10_1  = (inst >> 21) & 0x3FF;

            uint64_t comb = (off20 << 20) | (off19_12 << 12) | (off11 << 11) | (off10_1 << 1);
            int64_t offset = sext(comb, 21);

            if (rd != 0) {
                regs[rd] = pc - code.data();
            }

            pc = (pc - 1) + (offset / 4);
            DISPATCH();
        }

        exec_jalr : {
            word inst = *pc++;
            byte rd = (inst >> 7) & 0x1F;
            int64_t v1 = regs[((inst >> 15) & 0x1F)];
            int64_t immI = sext(((inst >> 20) & 0xFFF), 12);

            if (rd != 0) {
                regs[rd] = pc - code.data();
            }

            int64_t target_instruction_index = v1 + (immI / 4);
            pc = code.data() + target_instruction_index;

            DISPATCH();
        }

        exec_ecall: {
            pc++;
            int64_t service = regs[17];
            switch (service)
            {
                case EXIT: {
                    uint8_t exit_code = static_cast<uint8_t>(regs[10]);
                    free_protected_ram(protected_ram, RAM_SIZE);
                    std::exit(exit_code);
                    break;
                }
                case IPRINT: {
                    std::printf("%ld\n", regs[10]);
                    break;
                }
                case FPRINT: {
                    std::printf("%.15g\n", fregs[10]);
                    break;
                }
                case BPRINT: {
                    std::printf("%s\n", regs[10] ? "true" : "false");
                    break;
                }
                default: {
                    std::cerr << "[zon error]: unknown ecall service(" << service << ")\n";
                    free_protected_ram(protected_ram, RAM_SIZE);
                    std::exit(1);
                    break;
                }
            }
            
            DISPATCH();
        }

        unknown_op: {
            std::cerr << "Opcode desconocido" << std::endl;
            free_protected_ram(protected_ram, RAM_SIZE);
            return;
        }
    }
}