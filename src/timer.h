#pragma once

#include "types.h"

class Timer {
public:
    Timer() = default;

    // Call this every CPU step with the number of T-cycles just executed.
    // Returns true if a timer interrupt should be requested.
    bool tick(int cycles);

    // Called by the memory bus when the CPU reads a timer register.
    u8 read(u16 address) const;

    // Called by the memory bus when the CPU writes a timer register.
    void write(u16 address, u8 value);

private:
    // DIV — increments at 16384 Hz. Writing any value resets it to 0.
    // Internally we track the full 16-bit divider counter; DIV is the high byte.
    u16 div_counter_ = 0;

    // TIMA — timer counter. Increments at frequency set by TAC.
    // When it overflows (goes past 255) it resets to TMA and requests an interrupt.
    u8 tima_ = 0;

    // TMA — timer modulo. TIMA is reset to this value on overflow.
    u8 tma_ = 0;

    // TAC — timer control.
    // Bit 2: timer enabled (1) or stopped (0)
    // Bits 1-0: clock select
    //   00 = CPU clock / 1024  =  4096 Hz
    //   01 = CPU clock / 4     = 262144 Hz
    //   10 = CPU clock / 16    =  65536 Hz
    //   11 = CPU clock / 64    =  16384 Hz
    u8 tac_ = 0;

    // Accumulated cycles for TIMA increment tracking
    int tima_cycles_ = 0;

    // Returns the number of T-cycles between each TIMA increment
    int tima_period() const;
};