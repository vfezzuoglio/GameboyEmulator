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
        for (long i = 0; i < 50000000L; ++i) {
            cpu.step();

            // Serial port output — Blargg's test ROM writes results here
            // When the game writes to 0xFF02 with value 0x81, it means
            // "send the byte at 0xFF01 out the serial port"
            if (bus.read(0xFF02) == 0x81) {
                char c = static_cast<char>(bus.read(0xFF01));
                std::cout << c << std::flush;
                bus.write(0xFF02, 0x00);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "\nCPU error: " << e.what() << "\n";
        cpu.dump_state();
        return 1;
    }

    std::cout << "\n";
    cpu.dump_state();
    return 0;
}