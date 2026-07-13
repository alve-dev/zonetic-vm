#pragma once

#include "common.hpp"

#include <array>
#include <csetjmp>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#ifdef _WIN32
#   include <windows.h>
#else
#   include <sys/mman.h>
#   include <unistd.h>
#endif

namespace zonvm {

// ------------------------------------------------------------------
// Memory layout
//
//  0                    text_size          +rodata    +data   RAM_SIZE
//  |--- .text (code) ---|--- .rodata ---|--- .data ---|--- stack ---|
//                                          heap grows up ^  ^ stack grows down
//
// A guard page sits just below the stack area. Touching it triggers
// a SIGSEGV / access violation that the signal handler catches and
// converts into a clean "stack overflow" error.
// ------------------------------------------------------------------

static constexpr size_t RAM_SIZE    = 16ULL * 1024 * 1024;  // 16 MB
static constexpr size_t STACK_LIMIT = 128ULL * 1024;         // 128 KB guard area

// ------------------------------------------------------------------
// Signal / exception handling for stack overflow detection
// ------------------------------------------------------------------

#ifdef _WIN32
static std::jmp_buf execution_rescue_point;

inline LONG WINAPI _seh_handler(PEXCEPTION_POINTERS ei) {
    if (ei->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        std::cout << "\n[zon error]: Stack Overflow "
                  << "[ x_x] <(\"You exceeded the 128KB stack limit\")\n";
        std::longjmp(execution_rescue_point, 1);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

#else
static sigjmp_buf execution_rescue_point;

inline void _sigsegv_handler(int, siginfo_t*, void*) {
    std::cout << "\n[zon error]: Stack Overflow "
              << "[ x_x] <(\"You exceeded the 128KB stack limit\")\n";
    siglongjmp(execution_rescue_point, 1);
}
#endif

// ------------------------------------------------------------------
// RAM allocation and deallocation
//
// Uses VirtualAlloc on Windows and mmap on POSIX.
// A guard page is installed just below the stack area in both cases.
// ------------------------------------------------------------------

inline uint8_t* allocate_ram() {
#ifdef _WIN32
    auto* ram = reinterpret_cast<uint8_t*>(
        VirtualAlloc(nullptr, RAM_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!ram) { std::cerr << "[zon error]: VirtualAlloc failed\n"; std::exit(1); }

    SYSTEM_INFO si;
    GetSystemInfo(&si);
    size_t page         = si.dwPageSize;
    size_t guard_offset = (RAM_SIZE - STACK_LIMIT) & ~(page - 1);

    DWORD old;
    VirtualProtect(ram + guard_offset, page, PAGE_NOACCESS, &old);
    AddVectoredExceptionHandler(1, _seh_handler);
    return ram;

#else
    size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    auto*  ram  = reinterpret_cast<uint8_t*>(
        mmap(nullptr, RAM_SIZE, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (ram == MAP_FAILED) { std::cerr << "[zon error]: mmap failed\n"; std::exit(1); }

    size_t guard_offset = (RAM_SIZE - STACK_LIMIT) & ~(page - 1);
    if (mprotect(ram + guard_offset, page, PROT_NONE) == -1) {
        std::cerr << "[zon error]: mprotect failed\n"; std::exit(1);
    }

    struct sigaction sa{};
    sa.sa_sigaction = _sigsegv_handler;
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

// ------------------------------------------------------------------
// VM
// ------------------------------------------------------------------

struct VM {
    uint8_t*  ram = nullptr;
    uint32_t* pc  = nullptr;

    std::array<int64_t, REGISTER_COUNT> regs{};
    std::array<double,  REGISTER_COUNT> fregs{};

    uint32_t rodata_start = 0;
    uint32_t data_start   = 0;
    uint32_t heap_start   = 0;
    uint32_t heap_bump    = 0;

    std::vector<uint32_t> arena_stack;

    VM() = default;

    void load(
        uint32_t                    entry_point,
        const std::vector<uint8_t>& text,
        const std::vector<uint8_t>& rodata,
        const std::vector<uint8_t>& data_section
    ) {
        ram = allocate_ram();

        rodata_start = static_cast<uint32_t>(text.size());
        data_start   = rodata_start + static_cast<uint32_t>(rodata.size());
        heap_start   = data_start   + static_cast<uint32_t>(data_section.size());

        std::memcpy(ram,                text.data(),         text.size());
        std::memcpy(ram + rodata_start, rodata.data(),       rodata.size());
        std::memcpy(ram + data_start,   data_section.data(), data_section.size());

        // sp (x2) starts at the top of RAM, 16-byte aligned
        regs[2] = static_cast<int64_t>((RAM_SIZE - 16) & ~0xFULL);
        // gp (x3) points to the start of .data for global variable access
        regs[3] = data_start;

        heap_bump = heap_start;
        pc = reinterpret_cast<uint32_t*>(ram) + (entry_point / 4);
    }

    void run();

    // ------------------------------------------------------------------
    // Helpers used by run()
    // ------------------------------------------------------------------

    // Sign-extend val from bits width to 64 bits
    static inline int64_t sext(uint64_t val, int bits) {
        uint64_t m = 1ULL << (bits - 1);
        return static_cast<int64_t>((val ^ m) - m);
    }

    // FSGNJ/FSGNJN/FSGNJX: inject sign bit from bits2 into bits1
    template <typename T, typename U>
    static U perform_sign_injection(U bits1, U bits2, uint32_t rm) {
        constexpr U SIGN_BIT  = U(1) << (sizeof(U) * 8 - 1);
        constexpr U BODY_MASK = ~SIGN_BIT;
        if (rm == 0x00) return (bits1 & BODY_MASK) | ( bits2 & SIGN_BIT);           // FSGNJ
        if (rm == 0x01) return (bits1 & BODY_MASK) | ((bits2 & SIGN_BIT) ^ SIGN_BIT); // FSGNJN
        if (rm == 0x02) return  bits1 ^ (bits2 & SIGN_BIT);                           // FSGNJX
        return bits1;
    }
};

} // namespace zonvm