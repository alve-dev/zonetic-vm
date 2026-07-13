#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

#include "zon_vm.hpp"

using namespace zonvm;

// ------------------------------------------------------------------
// .zbc binary header
// ------------------------------------------------------------------

static const uint8_t MAGIC[6] = {'!', 'N', 'O', 'Z', 'o', '\0'};

struct Header {
    uint8_t  version;
    uint8_t  flags;
    uint32_t entry_point;
    uint32_t text_size;
    uint32_t data_size;
    uint32_t pool_size;
};

// ------------------------------------------------------------------
// Entry point
// ------------------------------------------------------------------

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: zonvm <file.zbc>\n";
        return 1;
    }

    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        std::cerr << "[zon error]: Could not open file: " << argv[1] << "\n";
        return 1;
    }

    // -- validate magic bytes --
    uint8_t magic[6];
    file.read(reinterpret_cast<char*>(magic), 6);
    if (std::memcmp(magic, MAGIC, 6) != 0) {
        std::cerr << "[zon error]: '" << argv[1] << "' is not a valid Zonetic bytecode file.\n";
        return 1;
    }

    // -- read header --
    Header hdr{};
    file.read(reinterpret_cast<char*>(&hdr.version),     1);
    file.read(reinterpret_cast<char*>(&hdr.flags),       1);
    file.read(reinterpret_cast<char*>(&hdr.entry_point), 4);
    file.read(reinterpret_cast<char*>(&hdr.text_size),   4);
    file.read(reinterpret_cast<char*>(&hdr.data_size),   4);
    file.read(reinterpret_cast<char*>(&hdr.pool_size),   4);
    file.seekg(40, std::ios::cur);  // reserved padding

    // -- read sections --
    std::vector<uint8_t> text(hdr.text_size);
    file.read(reinterpret_cast<char*>(text.data()), hdr.text_size);

    std::vector<uint8_t> pool(hdr.pool_size);
    if (hdr.pool_size > 0)
        file.read(reinterpret_cast<char*>(pool.data()), hdr.pool_size);

    std::vector<uint8_t> data(hdr.data_size);
    if (hdr.data_size > 0)
        file.read(reinterpret_cast<char*>(data.data()), hdr.data_size);

    // -- run --
    VM vm;
    vm.load(hdr.entry_point, text, pool, data);
    vm.run();

    return 0;
}