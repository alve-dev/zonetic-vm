#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include "zon_vm.hpp"
using namespace zonvm;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: zonvm <file.zbc>\n";
        return 1;
    }

    std::ifstream file(argv[1], std::ios::in | std::ios::binary);
    if (!file) {
        std::cerr << "No se pudo abrir el archivo: " << argv[1] << "\n";
        return 1;
    }

    const uint8_t EXPECTED_MAGIC[6] = {'!', 'N', 'O', 'Z', 'o', '\0'};
    uint8_t magic[6];
    file.read(reinterpret_cast<char*>(magic), 6);
    if (std::memcmp(magic, EXPECTED_MAGIC, 6) != 0) {
        std::cerr << "Error: El archivo no es un bytecode de Zonetic valido.\n";
        return 1;
    }

    uint8_t version;
    file.read(reinterpret_cast<char*>(&version), 1);

    uint8_t flags;
    file.read(reinterpret_cast<char*>(&flags), 1);

    uint32_t entry_point;
    file.read(reinterpret_cast<char*>(&entry_point), 4);

    uint32_t text_size;
    file.read(reinterpret_cast<char*>(&text_size), 4);

    uint32_t data_size;
    file.read(reinterpret_cast<char*>(&data_size), 4);

    uint32_t pool_size;
    file.read(reinterpret_cast<char*>(&pool_size), 4);

    file.seekg(40, std::ios::cur);

    VM vm;

    uint32_t instruction;
    uint32_t text_words = text_size / 4;
    for (uint32_t i = 0; i < text_words; i++) {
        file.read(reinterpret_cast<char*>(&instruction), 4);
        vm.code.push_back(instruction);
    }

    std::vector<uint8_t> pool_bytes(pool_size);
    if (pool_size > 0) {
        file.read(reinterpret_cast<char*>(pool_bytes.data()), pool_size);
    }

    std::vector<uint8_t> data_bytes(data_size);
    if (data_size > 0) {
        file.read(reinterpret_cast<char*>(data_bytes.data()), data_size);
    }

    vm.load(entry_point, data_bytes, pool_bytes);
    vm.run();

    return 0;
}