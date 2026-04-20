#include "memory_bus.h"
#include "cpu.h"
#include <iostream>
#include <string>
#include <SDL2/SDL.h>

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

    bool running = true;
    SDL_Event event;

    try {
        while (running) {
            // Handle window events
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    running = false;
                }
                if (event.type == SDL_KEYDOWN &&
                    event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                }
            }

            // Run one frame worth of cycles (~70224 T-cycles per frame)
            int frame_cycles = 0;
            while (frame_cycles < 70224) {
                int cycles = cpu.step();
                bus.tick_timer(cycles);
                bool frame_done = bus.tick_ppu(cycles);
                frame_cycles += cycles;

                // Serial output for test ROMs
                if (bus.read(0xFF02) == 0x81) {
                    char c = static_cast<char>(bus.read(0xFF01));
                    std::cout << c << std::flush;
                    bus.write(0xFF02, 0x00);
                }

                if (frame_done) break;
            }

            // Present the frame to the screen
            PPU& ppu = bus.ppu();
            SDL_UpdateTexture(
                ppu.texture_, nullptr,
                ppu.framebuffer().data(),
                SCREEN_WIDTH * sizeof(u32)
            );
            SDL_RenderClear(ppu.renderer_);
            SDL_RenderCopy(ppu.renderer_, ppu.texture_, nullptr, nullptr);
            SDL_RenderPresent(ppu.renderer_);
        }
    } catch (const std::exception& e) {
        std::cerr << "\nCPU error: " << e.what() << "\n";
        cpu.dump_state();
        return 1;
    }

    return 0;
}