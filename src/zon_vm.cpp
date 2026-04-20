#include "zon_vm.hpp"
#include <iostream>

namespace zonvm {
    void VM::run() {
        ip = code.data();
        word* end = ip + code.size();

        while (ip < end) {
            word instruction = *ip++;

            byte opcode = instruction & 0x7F;
            int rd      = (instruction >> 7) & 0x1F;
            int funct3  = (instruction >> 12) & 0x7;
            int rs1     = (instruction >> 15) & 0x1F;
            int rs2     = (instruction >> 20) & 0x1F;
            int funct7  = (instruction >> 25) & 0x7F;
            
            word imm = (instruction >> 20) & 0xFFF;
            if (imm & 0x800) imm |= 0xFFFFF000;

            switch (opcode) {
                case OP_IMM:
                    write_reg(rd, read_reg(rs1) + static_cast<double>(imm));
                    break;

                case OP:
                    if (funct7 == M_EXT) {
                        switch (funct3) {
                            case MUL:
                                write_reg(rd, read_reg(rs1) * read_reg(rs2));
                                break;
                            
                            case DIV:
                                if (read_reg(rs2) != 0) write_reg(rd, read_reg(rs1) / read_reg(rs2));
                                else write_reg(rd, 0);
                                break;

                            case REM:
                                if (read_reg(rs2) != 0) write_reg(rd, (int)read_reg(rs1) % (int)read_reg(rs2));
                                else write_reg(rd, 0);
                                break;
                        }
                    } else if (funct3 == ADD_SUB) {
                        if (funct7 == ADD) {
                            write_reg(rd, read_reg(rs1) + read_reg(rs2));
                        } else if (funct7 == SUB) {
                            write_reg(rd, read_reg(rs1) - read_reg(rs2));
                        }
                    }
                    break;


                case HALT:
                    return;

                default:
                    std::cerr << "Opcode desconocido: " << (int)opcode << std::endl;
                    return;
            }
        }
    }
}