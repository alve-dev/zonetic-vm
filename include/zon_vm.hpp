#pragma once
#include "common.hpp"
#include <array>
#include <cstdint>
#include <vector>
#include <cstring>
#include <iostream>
#include <csignal>
#include <csetjmp>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace zonvm {

    static constexpr size_t RAM_SIZE    = 1024ULL * 1024 * 16; 
    static constexpr size_t STACK_LIMIT = 1024ULL * 128;

    #ifdef _WIN32
    static std::jmp_buf execution_rescue_point;
    #else
    static sigjmp_buf execution_rescue_point;
    #endif

    #ifdef _WIN32
    inline LONG WINAPI zonetic_windows_exception_handler(PEXCEPTION_POINTERS ei) {
        if (ei->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
            std::cout << "\n[zon error]: Stack Overflow, [X_X] <(\"You exceeded the 128KB stack limit\")\n";
            std::longjmp(execution_rescue_point, 1);
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }
    #else
    inline void zonetic_stack_overflow_handler(int, siginfo_t*, void*) {
        std::cout << "\n[zon error]: Stack Overflow, [X_X] <(\"You exceeded the 128KB stack limit\")\n";
        siglongjmp(execution_rescue_point, 1); // Restaura las señales del SO
    }
    #endif

    inline uint8_t* allocate_ram() {
    #ifdef _WIN32
        uint8_t* ram = reinterpret_cast<uint8_t*>(
            VirtualAlloc(nullptr, RAM_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!ram) { std::cerr << "[zon error]: VirtualAlloc failed\n"; std::exit(1); }

        SYSTEM_INFO si; GetSystemInfo(&si);
        size_t page = si.dwPageSize;
        
        size_t guard_offset = (RAM_SIZE - STACK_LIMIT) & ~(page - 1);
        
        DWORD old; 
        VirtualProtect(ram + guard_offset, page, PAGE_NOACCESS, &old);
        AddVectoredExceptionHandler(1, zonetic_windows_exception_handler);
        return ram;
    #else
        size_t page = sysconf(_SC_PAGESIZE);
        uint8_t* ram = reinterpret_cast<uint8_t*>(
            mmap(nullptr, RAM_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        if (ram == MAP_FAILED) { std::cerr << "[zon error]: mmap failed\n"; std::exit(1); }

        size_t guard_offset = (RAM_SIZE - STACK_LIMIT) & ~(page - 1);
        
        if (mprotect(ram + guard_offset, page, PROT_NONE) == -1) {
            std::cerr << "[zon error]: mprotect failed\n"; std::exit(1);
        }

        struct sigaction sa{};
        sa.sa_sigaction = zonetic_stack_overflow_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
        sigaction(SIGSEGV, &sa, nullptr);
        return ram;
    #endif
    }

    inline void free_ram(uint8_t* ram) {
        if (!ram) return;
    #ifdef _WIN32
        VirtualFree(ram, 0, MEM_RELEASE);
    #else
        munmap(ram, RAM_SIZE);
    #endif
    }

    struct VM {
        uint8_t* ram = nullptr;
        uint32_t* pc = nullptr;

        std::array<int64_t, REGISTER_COUNT> regs{};
        std::array<double,  REGISTER_COUNT> fregs{};
        uint32_t fcsr = 0;

        uint32_t rodata_start = 0;
        uint32_t data_start   = 0;
        uint32_t heap_start   = 0;

        uint32_t              heap_bump = 0;
        std::vector<uint32_t> arena_stack; 

        VM() = default;

        void load(uint32_t entry_point,
                  const std::vector<uint8_t>& text,
                  const std::vector<uint8_t>& rodata,
                  const std::vector<uint8_t>& data_section) {

            ram = allocate_ram();

            rodata_start = static_cast<uint32_t>(text.size());
            data_start   = rodata_start + static_cast<uint32_t>(rodata.size());
            heap_start   = data_start   + static_cast<uint32_t>(data_section.size());

            std::memcpy(ram,                        text.data(),         text.size());
            std::memcpy(ram + rodata_start,         rodata.data(),       rodata.size());
            std::memcpy(ram + data_start,           data_section.data(), data_section.size());

            regs[2] = static_cast<int64_t>((RAM_SIZE - 16) & ~0xF); 
            regs[3] = data_start; // gp

            heap_bump = heap_start;
            pc = reinterpret_cast<uint32_t*>(ram) + (entry_point / 4);
        }

        void run();

        static inline int64_t sext(uint64_t val, int bits) {
            uint64_t m = 1ULL << (bits - 1);
            return static_cast<int64_t>((val ^ m) - m);
        }

        template <typename T, typename U>
        U perform_sign_injection(U bits1, U bits2, uint32_t rm) {
            U sign_bit  = (U)1 << (sizeof(U) * 8 - 1);
            U body_mask = ~sign_bit;
            if      (rm == 0x00) return (bits1 & body_mask) | (bits2 & sign_bit);
            else if (rm == 0x01) return (bits1 & body_mask) | ((bits2 & sign_bit) ^ sign_bit);
            else if (rm == 0x02) return bits1 ^ (bits2 & sign_bit);
            return bits1;
        }
    };

} // namespace zonvm
