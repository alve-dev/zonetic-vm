#pragma once
#include "common.hpp"
#include "zon_value.hpp"
#include <vector>

namespace zonvm {
    struct VM {
        std::vector<word> code;
        word* pc;
        Value registers[REGISTER_COUNT];

        VM() : pc(nullptr) {
            for (int i = 0; i < REGISTER_COUNT; ++i) {
                registers[i] = Value(0);
            }
        }

        void write_reg(byte reg, int32_t val) {
            if (reg == 0) return; 
            registers[reg].number = val;
        }

        int32_t read_reg(byte reg) {
            if (reg == 0) return 0;
            return registers[reg].number;
        }
        void run();
    };
}