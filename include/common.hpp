#pragma once
#include <cstdint>

namespace zonvm {
    using byte = uint8_t;
    using word = uint32_t;

    static constexpr byte REGISTER_COUNT = 32;

    enum OpCode : byte {
        OP_L = 0x03,
        FL = 0x07,
        OP_STR = 0x0B,
        OP_IMM = 0x13,
        AUIPC = 0x17,
        OP_IMM_32 = 0x1B,
        OP_S = 0x23,
        OP_IMM_STR = 0x2B,
        OP_FS = 0x27,
        OP = 0x33,
        OP_32 = 0x3B,
        LUI = 0x37,
        OP_F = 0x53,
        OP_B = 0x63,
        JALR = 0x67,
        JAL = 0x6F,
        ECALL = 0x73
    };

    enum F3_ALU : byte {
        ADD_SUB  = 0x00,
        SLL_SLLI = 0x01,
        SLT_SLTI = 0x02,
        SLTU_SLTIU = 0x03,
        XOR_XORI = 0x04,
        SRL_SRA_SRLI_SRAI = 0x05,
        OR_ORI = 0x06,
        AND_ANDI = 0x07
    };

    enum F3_M_EXT : byte {
        MUL = 0x00,
        DIV = 0x04,
        REM = 0x06
    };

    enum F3_FL : byte {
        FLD = 0x3
    };

    enum F3_L : byte {
        LD = 0x3
    };

    enum F3_S: byte {
        SD = 0x3
    };

    enum F3_FS: byte {
        FSD = 0x3
    };

    enum F3_B : byte {
        BEQ = 0x00,
        BNE = 0x01,
        BLT = 0x04,
        BGE = 0x05,
        BLTU = 0x06,
        BGEU = 0x07
    };

    enum F3_STR : byte {
        CONCAT = 0x00,
        EQ_STR = 0x01,
    };

    enum F7_STR : byte {
        STANDARD_STR = 0x00
    };

    enum F7 : byte {
        STANDARD = 0x00,
        M_EXT_OR_FADD_D = 0x01,
        FSUB_S = 0x04,
        FSUB_D = 0x05,
        FMUL_S = 0x08,
        FMUL_D = 0x09,
        FDIV_S = 0x0C,
        FDIV_D = 0x0D,
        FSGNJ_S = 0x10,
        FSGNJ_D = 0x11,
        ALT = 0x20,
        FCOMP_S = 0x50,
        FCOMP_D = 0x51,
        FCVT_S_W = 0x68,
        FCVT_D_L = 0x69,
        FMV_D_X = 0x71,
        FMV_W_X = 0x78
    };

    enum SysCalls {
        IPRINT = -1,
        FPRINT = -2,
        BPRINT = -3,
        SPRINT = -4,
        EPRINT = -5,
        HEAP_PUSH = -100,
        HEAP_POP = -101,
        HEAP_ALLOC = -102,
        HEAP_STORE = -103,
        HEAP_LOAD = -104,
        FILL_ZERO_STACK = -200,
        OUT_BOUND_INDEX_ERR = -900,
        EXIT = 93
    };
} 