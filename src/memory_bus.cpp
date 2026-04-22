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
    if (address >= 0x8000 && address < 0xA000) {
        return ppu_.read(address);
    }
    if (address >= 0xFE00 && address < 0xFEA0) {
        return ppu_.read(address);
    }
    if (address >= 0xFF03 && address <= 0xFF07) {
        return timer_.read(address);
    }
    if (address >= 0xFF40 && address <= 0xFF4B) {
        return ppu_.read(address);
    }
    if (address == 0xFF00) {
        u8 result = 0xCF;
        if (!(joypad_select_ & 0x10)) {
            result &= 0xF0;
            result |= (joypad_state_ >> 4) & 0x0F;
        }
        if (!(joypad_select_ & 0x20)) {
            result &= 0xF0;
            result |= joypad_state_ & 0x0F;
        }
        return result;
    }
    return mem_[address];
}

void MemoryBus::write(u16 address, u8 value) {
    if (address < 0x8000) {
        if (address < 0x2000) {
            ram_enabled_ = ((value & 0x0F) == 0x0A);
        } else if (address < 0x4000) {
            u8 bank = value & 0x1F;
            if (bank == 0) bank = 1;
            rom_bank_ = bank;
        }
        return;
    }
    if (address >= 0x8000 && address < 0xA000) {
        ppu_.write(address, value);
        return;
    }
    if (address >= 0xFE00 && address < 0xFEA0) {
        ppu_.write(address, value);
        return;
    }
    if (address >= 0xFF03 && address <= 0xFF07) {
        timer_.write(address, value);
        return;
    }
    if (address >= 0xFF40 && address <= 0xFF4B) {
        ppu_.write(address, value);
        return;
    }
    if (address == 0xFF00) {
        joypad_select_ = value;
        return;
    }
    mem_[address] = value;
}

u16 MemoryBus::read16(u16 address) const {
    u16 lo = read(address);
    u16 hi = static_cast<u16>(read(address + 1));
    return static_cast<u16>((hi << 8) | lo);
}

void MemoryBus::write16(u16 address, u16 value) {
    write(address,     static_cast<u8>(value & 0xFF));
    write(address + 1, static_cast<u8>((value >> 8) & 0xFF));
}

bool MemoryBus::tick_timer(int cycles) {
    bool interrupt = timer_.tick(cycles);
    if (interrupt) {
        mem_[0xFF0F] |= 0x04;
    }
    return interrupt;
}

bool MemoryBus::tick_ppu(int cycles) {
    bool vblank = ppu_.tick(cycles);
    if (vblank) {
        mem_[0xFF0F] |= 0x01;
    }
    return vblank;
}