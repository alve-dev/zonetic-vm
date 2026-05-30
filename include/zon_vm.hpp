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
        std::vector<uint8_t> data;
        std::vector<uint8_t> pool_data;

        static inline int64_t sext(uint64_t val, int bits) {
            uint64_t m = 1ULL << (bits - 1);
            return (int64_t)((val ^ m) - m);
        }

        void load(uint32_t entry, const std::vector<uint8_t>& data_section, const std::vector<uint8_t>& pool_section) {
            // .data es su propio vector
            data = data_section;
            pool_data = pool_section;

            // gp apunta al centro de .data para maximizar rango ±2048
            regs[3] = 0;

            // entry point como offset de instrucción
            pc = code.data() + (entry / 4);
        }

        void run();

        template <typename T, typename U>
        U perform_sign_injection(U bits1, U bits2, uint32_t rm) {
            U sign_bit = (U)1 << (sizeof(U) * 8 - 1);
            U body_mask = ~sign_bit;

            if (rm == 0x00)      return (bits1 & body_mask) | (bits2 & sign_bit);
            else if (rm == 0x01) return (bits1 & body_mask) | ((bits2 & sign_bit) ^ sign_bit);
            else if (rm == 0x02) return bits1 ^ (bits2 & sign_bit);
            return bits1;
        }
    };
} 