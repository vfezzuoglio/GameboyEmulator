#include "memory_bus.h"
#include <fstream>
#include <iterator>
#include <stdexcept>

MemoryBus::MemoryBus() {
    mem_.fill(0x00);
}

void MemoryBus::load_rom(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open ROM: " + path);
    }

    rom_ = std::vector<u8>(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );

    if (rom_.size() < 0x150) {
        throw std::runtime_error("ROM too small to be valid");
    }

    for (std::size_t i = 0; i < std::min(rom_.size(), std::size_t{0x8000}); ++i) {
        mem_[i] = rom_[i];
    }
}


u8 MemoryBus::read(u16 address) const {
    if (address < 0x8000) {
        if (address < 0x4000) {
            return rom_[address];
        }
        u32 offset = static_cast<u32>(rom_bank_) * 0x4000;
        u32 local = address - 0x4000;
        return rom_[offset + local];
    }
    return mem_[address];
}

void MemoryBus::write(u16 address, u8 value) {
    if (address <0x8000) {
        if (address < 0x2000) {
            ram_enabled_ = ((value & 0x0F) == 0x0A);
        } else if (address < 0x4000) {
            u8 bank = value & 0x1F;
            if (bank == 0) bank = 1;
            rom_bank_ = bank;
        }
        return;
    }
    mem_[address] = value;
}

u16 MemoryBus::read16(u16 address) const {
    u16 lo = read(address);
    u16 hi = static_cast<u16>(read(address + 1));
    return static_cast<u16>((hi << 8) | lo);
}

void MemoryBus::write16(u16 address, u16 value){
    write(address,     static_cast<u8>(value & 0xFF));
    write(address + 1, static_cast<u8>((value >> 8) & 0xFF));
}

