#pragma once
#include "common.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace zonvm {
    struct VM {
        std::vector<uint32_t> code;
        uint32_t* pc;
        std::array<int64_t, REGISTER_COUNT> regs{};
        std::array<double, REGISTER_COUNT> fregs{};
        uint32_t fcsr = 0;

        static inline int64_t sext(uint64_t val, int bits) {
            uint64_t m = 1ULL << (bits - 1);
            return (int64_t)((val ^ m) - m);
        }

        void run();
    };
}