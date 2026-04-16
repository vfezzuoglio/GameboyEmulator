#pragma once

#include "types.h"
#include "memory_bus.h"

class CPU {
public:
    explicit CPU(MemoryBus& bus);

    int  step();
    void reset();
    void dump_state() const;

    u8   get_a()  const { return a_; }
    u16  get_pc() const { return pc_; }
    u16  get_sp() const { return sp_; }
    bool is_halted() const { return halted_; }


private:
    MemoryBus& bus_;

    u8 a_ = 0x01;
    u8 f_ = 0xB0;
    u8 b_ = 0x00;
    u8 c_ = 0x13;
    u8 d_ = 0x00;
    u8 e_ = 0xD8;
    u8 h_ = 0x01;
    u8 l_ = 0x4D;

    u16 sp_      = 0xFFFE;
    u16 pc_      = 0x0100;
    bool halted_   = false;
    bool ime_      = false;
    bool ime_next_ = false;

    static constexpr u8 Z_FLAG = 0x80;
    static constexpr u8 N_FLAG = 0x40;
    static constexpr u8 H_FLAG = 0x20;
    static constexpr u8 C_FLAG = 0x10;

    u16 af() const { return static_cast<u16>((a_ << 8) | f_); }
    u16 bc() const { return static_cast<u16>((b_ << 8) | c_); }
    u16 de() const { return static_cast<u16>((d_ << 8) | e_); }
    u16 hl() const { return static_cast<u16>((h_ << 8) | l_); }

    void set_af(u16 v) { a_ = static_cast<u8>(v >> 8); f_ = static_cast<u8>(v & 0xF0); }
    void set_bc(u16 v) { b_ = static_cast<u8>(v >> 8); c_ = static_cast<u8>(v & 0xFF); }
    void set_de(u16 v) { d_ = static_cast<u8>(v >> 8); e_ = static_cast<u8>(v & 0xFF); }
    void set_hl(u16 v) { h_ = static_cast<u8>(v >> 8); l_ = static_cast<u8>(v & 0xFF); }

    bool flag_z() const { return (f_ & Z_FLAG) != 0; }
    bool flag_n() const { return (f_ & N_FLAG) != 0; }
    bool flag_h() const { return (f_ & H_FLAG) != 0; }
    bool flag_c() const { return (f_ & C_FLAG) != 0; }

    void set_flag_z(bool v) { v ? (f_ |= Z_FLAG) : (f_ &= ~Z_FLAG); }
    void set_flag_n(bool v) { v ? (f_ |= N_FLAG) : (f_ &= ~N_FLAG); }
    void set_flag_h(bool v) { v ? (f_ |= H_FLAG) : (f_ &= ~H_FLAG); }
    void set_flag_c(bool v) { v ? (f_ |= C_FLAG) : (f_ &= ~C_FLAG); }

    void set_flags(bool z, bool n, bool h, bool c) {
        set_flag_z(z); set_flag_n(n); set_flag_h(h); set_flag_c(c);
    }

    u8  fetch8();
    u16 fetch16();
    void push(u16 value);
    u16  pop();

    u8 alu_add(u8 a, u8 b, bool carry = false);
    u8 alu_sub(u8 a, u8 b, bool carry = false);
    u8 alu_and(u8 a, u8 b);
    u8 alu_or (u8 a, u8 b);
    u8 alu_xor(u8 a, u8 b);
    u8 alu_inc(u8 val);
    u8 alu_dec(u8 val);

    int execute(u8 opcode);
    int execute_cb(u8 opcode);
    int handle_interrupts();
};