#include "memory_bus.h"
#include "cpu.h"
#include <iostream>
#include <string>
#include <SDL2/SDL.h>
#include <chrono>
#include <thread>

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
                            if (event.type == SDL_QUIT) running = false;
                            if (event.type == SDL_KEYDOWN &&
                                event.key.keysym.sym == SDLK_ESCAPE) running = false;

                            if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                                bool pressed = (event.type == SDL_KEYDOWN);

                                // joypad_state_ packs all 8 buttons into one byte.
                                // Low nibble = buttons: A, B, Select, Start (bits 0-3)
                                // High nibble = d-pad:  Right, Left, Up, Down (bits 4-7)
                                // 0 = pressed, 1 = not pressed (inverted logic)


                                switch (event.key.keysym.sym) {
                                case SDLK_z:         pressed ? bus.button_down(0) : bus.button_up(0); break; // A
                                case SDLK_x:         pressed ? bus.button_down(1) : bus.button_up(1); break; // B
                                case SDLK_BACKSPACE: pressed ? bus.button_down(2) : bus.button_up(2); break; // Select
                                case SDLK_RETURN:    pressed ? bus.button_down(3) : bus.button_up(3); break; // Start
                                case SDLK_RIGHT:     pressed ? bus.button_down(4) : bus.button_up(4); break; // Right
                                case SDLK_LEFT:      pressed ? bus.button_down(5) : bus.button_up(5); break; // Left
                                case SDLK_UP:        pressed ? bus.button_down(6) : bus.button_up(6); break; // Up
                                case SDLK_DOWN:      pressed ? bus.button_down(7) : bus.button_up(7); break; // Down
                                default: break;
                            }

                            if (pressed) {
                                bus.write(0xFF0F, bus.read(0xFF0F) | 0x10);
                            }
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

            // Frame limiter — target 59.73fps
            static auto last_frame = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            auto frame_duration = std::chrono::microseconds(16742); // 1/59.73 seconds
            auto elapsed = now - last_frame;
            if (elapsed < frame_duration) {
                std::this_thread::sleep_for(frame_duration - elapsed);
            }
            last_frame = std::chrono::steady_clock::now();
        }
    } catch (const std::exception& e) {
        std::cerr << "\nCPU error: " << e.what() << "\n";
        cpu.dump_state();
        return 1;
    }

    return 0;
}