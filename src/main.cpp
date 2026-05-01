#include <iostream>
#include <fstream>
#include <vector>
#include "zon_vm.hpp"

using namespace zonvm;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: zonvm <file.zbc>" << std::endl;
        return 1;
    }

    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        std::cerr << "No se pudo abrir el archivo: " << argv[1] << std::endl;
        return 1;
    }

    uint32_t magic;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (magic != 0x5A4F4E21) {
        std::cerr << "Error: El archivo no es un bytecode de Zonetic valido." << std::endl;
        return 1;
    }

    VM vm;
    uint32_t instruction;
    while (file.read(reinterpret_cast<char*>(&instruction), sizeof(instruction))) {
        vm.code.push_back(instruction);
    }

    vm.run();

    /*for (int i = 0; i < 32; ++i) {
        std::cout << "x" << i << ": " << vm.regs[i] << std::endl;
    }*/

    return 0;
}