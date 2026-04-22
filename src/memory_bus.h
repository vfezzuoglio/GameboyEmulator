#pragma once   

#include "types.h"
#include <array>
#include <vector>
#include <string>
#include <stdexcept>
#include "timer.h"
#include "ppu.h"


struct CartridgeHeader {
    std::string title;
    u8          rom_banks;
    u8          ram_banks;
    bool        has_battery;
};

class MemoryBus {
public:
    explicit MemoryBus();
    ~MemoryBus() = default;
    MemoryBus(const MemoryBus&)            = delete;
    MemoryBus& operator=(const MemoryBus&) = delete;
    MemoryBus(MemoryBus&&)                 = delete;
    MemoryBus& operator=(MemoryBus&&)      = delete;
    bool tick_timer(int cycles);

    void load_rom(const std::string& path);

    u8 read(u16 address) const;
    void write(u16 address, u8 value);
    u16 read16(u16 address) const;
    void write16(u16 address, u16 value);
    bool tick_ppu(int cycles);
    PPU& ppu() { return ppu_; }

    const CartridgeHeader& header() const { return header_; }
    void button_down(u8 bit)  { joypad_state_ &= ~(1 << bit); }
    void button_up(u8 bit)    { joypad_state_ |=  (1 << bit); }

private:
    std::array<u8, 0x10000> mem_{};
    std::vector<u8>         rom_{};
    CartridgeHeader         header_{};
    u8   rom_bank_ = 1;
    bool ram_enabled_ = false;
    Timer timer_;
    PPU ppu_;
    u8 joypad_state_ = 0xFF; // All buttons unpressed
    u8 joypad_select_ = 0xFF;


};