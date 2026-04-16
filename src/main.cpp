#include "memory_bus.h"
#include "cpu.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: gbemu <rom.gb>\n";
        return 1;
    }

    std::string rom_path = argv[1];

    MemoryBus bus;

    try {
        bus.load_rom(rom_path);
    } catch (const std::exception& e) {
        std::cerr << "Failed to load ROM: " << e.what() << "\n";
        return 1;
    }

    std::cout << "Loaded: " << bus.header().title << "\n";

    CPU cpu(bus);

    try {
        for (int i = 0; i < 100000; ++i) {
            cpu.step();
        }
    } catch (const std::exception& e) {
        std::cerr << "CPU error: " << e.what() << "\n";
        cpu.dump_state();
        return 1;
    }

    cpu.dump_state();
    return 0;
}