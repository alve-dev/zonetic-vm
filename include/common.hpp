#pragma once
#include <cstdint>

namespace zonvm {
    using byte = uint8_t;
    using word = uint32_t;

    static constexpr byte REGISTER_COUNT = 32;

    enum OpCode : byte {
        OP_IMM = 0x13,
        OP = 0x33,
        LUI = 0x37,
        OP_F = 0x53,
        OP_B = 0x63,
        JAL = 0x6F,
        ECALL = 0x73
    };

    enum F3_ALU : byte {
        ADD_SUB  = 0x00,
        SLT_SLTI = 0x02,
        SLTU_SLTIU = 0x03,
        XOR_XORI = 0x04,
        OR_ORI = 0x06,
        AND_ANDI = 0x07
    };

    enum F3_M_EXT : byte {
        MUL = 0x00,
        DIV = 0x04,
        REM = 0x06
    };

    enum F3_B : byte {
        BEQ = 0x00,
        BNE = 0x01,
        BLT = 0x04,
        BGE = 0x05,
        BLTU = 0x06,
        BGEU = 0x07
    };

    enum F7 : byte {
        STANDARD = 0x00,
        M_EXT = 0x01,
        FSUB_S = 0x04,
        FMUL_S = 0x08,
        FDIV_S = 0x0C,
        FSGNJ_S = 0x10,
        ALT = 0x20,
        FCOMP_S = 0x50,
        FCVT_S_W = 0x68,
        FMV_W_X = 0x78
    };

    enum SysCalls {
        IPRINT = 1000,
        FPRINT = 1001,
        BPRINT = 1002,
        EXIT = 93
    };
}