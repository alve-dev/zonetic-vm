#pragma once
#include "common.hpp"
#include "zon_value.hpp"
#include <vector>

namespace zonvm {
    struct VM {
        std::vector<word> code;
        word* ip;
        Value registers[REGISTER_COUNT];

        VM() : ip(nullptr) {
            for (int i = 0; i < REGISTER_COUNT; ++i) {
                registers[i] = Value(0.0);
            }
        }

        void write_reg(int reg, double val) {
            if (reg == 0) return; 
            registers[reg].number = val;
        }

        double read_reg(int reg) {
            if (reg == 0) return 0.0;
            return registers[reg].number;
        }
        void run();
    };
}