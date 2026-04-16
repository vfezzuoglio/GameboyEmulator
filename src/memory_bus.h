#pragma once   

#include "types.h"
#include <array>
#include <vector>
#include <string>
#include <stdexcept>


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

    void load_rom(const std::string& path);

    u8 read(u16 address) const;
    void write(u16 address, u8 value);
    u16 read16(u16 address) const;
    void write16(u16 address, u16 value);

    const CartridgeHeader& header() const { return header_; }

private:
    std::array<u8, 0x10000> mem_{};
    std::vector<u8>         rom_{};
    CartridgeHeader         header_{};
    u8   rom_bank_ = 1;
    bool ram_enabled_ = false;


};