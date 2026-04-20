#pragma once
#include <cstdint>

namespace zonvm {
    using byte = uint8_t;
    using word = uint32_t;

    static constexpr int REGISTER_COUNT = 32;

    enum OpCode : byte {
        OP_IMM = 0x13,
        OP     = 0x33,
        HALT = 0x00
    };

    enum Funct3 : byte {
        ADD_SUB = 0x00,
        MUL = 0x00,
        ADDI = 0x01,
        DIV = 0x04,
        REM = 0x06
    };
    enum Funct7 : byte {
        ADD = 0x00,
        M_EXT = 0x01,
        SUB = 0x20,

    };
}