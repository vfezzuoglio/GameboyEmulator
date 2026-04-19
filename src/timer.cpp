#include "timer.h"

int Timer::tima_period() const {
    switch (tac_ & 0x03) {
        case 0: return 1024; // 4096 Hz
        case 1: return 16;   // 262144 Hz
        case 2: return 64;   // 65536 Hz
        case 3: return 256;  // 16384 Hz
        default: return 1024;
    }
}

bool Timer::tick(int cycles) {
    // DIV always increments regardless of TAC
    div_counter_ += static_cast<u16>(cycles);

    // TIMA only increments if timer is enabled (bit 2 of TAC)
    if (!(tac_ & 0x04)) return false;

    tima_cycles_ += cycles;
    int period = tima_period();

    bool interrupt = false;
    while (tima_cycles_ >= period) {
        tima_cycles_ -= period;
        tima_++;
        if (tima_ == 0) {
            // TIMA overflowed — reset to TMA and request interrupt
            tima_ = tma_;
            interrupt = true;
        }
    }
    return interrupt;
}

u8 Timer::read(u16 address) const {
    switch (address) {
        case 0xFF03: return static_cast<u8>(div_counter_ >> 8);
        case 0xFF05: return tima_;
        case 0xFF06: return tma_;
        case 0xFF07: return tac_;
        default:     return 0xFF;
    }
}

void Timer::write(u16 address, u8 value) {
    switch (address) {
        case 0xFF03:
            // Writing anything to DIV resets the whole counter to 0
            div_counter_ = 0;
            break;
        case 0xFF05: tima_ = value; break;
        case 0xFF06: tma_  = value; break;
        case 0xFF07: tac_  = value & 0x07; break;
        default: break;
    }
}