#include "cpu.h"
#include <iostream>
#include <stdexcept>
#include <string>

CPU::CPU(MemoryBus& bus)
    : bus_(bus) 
{
    reset();
}

void CPU::reset() {
    a_ = 0x01; f_ = 0xB0;
    b_ = 0x00; c_ = 0x13;
    d_ = 0x00; e_ = 0xD8;
    h_ = 0x01; l_ = 0x4D;
    sp_       = 0xFFFE;
    pc_       = 0x0100;
    halted_   = false;
    ime_      = false;
    ime_next_ = false;    
}

int CPU::step() {
    int cycles = handle_interrupts();
    if (cycles > 0) return cycles;

    if (halted_) return 4;

    if (ime_next_) {
        ime_      = true;
        ime_next_ = false;
    }

    u8 opcode = fetch8();
    return execute(opcode);
}

u8 CPU::fetch8() {
    u8 val = bus_.read(pc_);
    ++pc_;
    return val;
}

u16 CPU::fetch16() {
    u16 val = bus_.read16(pc_);
    pc_ += 2;
    return val;
}

void CPU::push(u16 value) {
    sp_ -= 2;
    bus_.write16(sp_, value);
}

u16 CPU::pop() {
    u16 value = bus_.read16(sp_);
    sp_ += 2;
    return value;
}

int CPU::handle_interrupts() {
    if (!ime_ && !halted_) return 0;

    u8 ie      = bus_.read(0xFFFF);
    u8 ifl     = bus_.read(0xFF0F);
    u8 pending = ie & ifl;
    if (!pending) return 0;

    halted_ = false;
    if (!ime_) return 0;

    ime_ = false;

    u8 bit = 0;
    while (bit < 5 && !((pending >> bit) & 1)) ++bit;

    bus_.write(0xFF0F, ifl & ~static_cast<u8>(1 << bit));

    static constexpr u16 VECTORS[] = {
        0x0040, 0x0048, 0x0050, 0x0058, 0x0060
    };

    push(pc_);
    pc_ = VECTORS[bit];
    return 20;
}

u8 CPU::alu_add(u8 a, u8 b, bool carry) {
    u32 c   = carry ? 1u : 0u;
    u32 res = static_cast<u32>(a) + b + c;
    bool half = ((a & 0x0F) + (b & 0x0F) + c) > 0x0F;
    set_flags((res & 0xFF) == 0, false, half, res > 0xFF);
    return static_cast<u8>(res & 0xFF);
}

u8 CPU::alu_sub(u8 a, u8 b, bool carry) {
    u32 c   = carry ? 1u : 0u;
    u32 res = static_cast<u32>(a) - b - c;
    bool half = static_cast<int>(a & 0x0F) - static_cast<int>(b & 0x0F) - static_cast<int>(c) < 0;
    set_flags((res & 0xFF) == 0, true, half, res > 0xFF);
    return static_cast<u8>(res & 0xFF);
}

u8 CPU::alu_and(u8 a, u8 b) {
    u8 res = a & b;
    set_flags(res == 0, false, true, false);
    return res;
}

u8 CPU::alu_or(u8 a, u8 b) {
    u8 res = a | b;
    set_flags(res == 0, false, false, false);
    return res;
}

u8 CPU::alu_xor(u8 a, u8 b) {
    u8 res = a ^ b;
    set_flags(res == 0, false, false, false);
    return res;
}

u8 CPU::alu_inc(u8 val) {
    bool half = (val & 0x0F) == 0x0F;
    ++val;
    set_flag_z(val == 0);
    set_flag_n(false);
    set_flag_h(half);
    return val;
}

u8 CPU::alu_dec(u8 val) {
    bool half = (val & 0x0F) == 0x00;
    --val;
    set_flag_z(val == 0);
    set_flag_n(true);
    set_flag_h(half);
    return val;
}

int CPU::execute_cb(u8 op) {
    u8 reg = op & 0x07;
    u8 bit = (op >> 3) & 0x07;
    u8 row = op >> 6;

    auto get_reg = [&]() -> u8 {
        switch (reg) {
            case 0: return b_;
            case 1: return c_;
            case 2: return d_;
            case 3: return e_;
            case 4: return h_;
            case 5: return l_;
            case 6: return bus_.read(hl());
            case 7: return a_;
            default: return 0;
        }
    };

    auto set_reg = [&](u8 val) {
        switch (reg) {
            case 0: b_ = val; break;
            case 1: c_ = val; break;
            case 2: d_ = val; break;
            case 3: e_ = val; break;
            case 4: h_ = val; break;
            case 5: l_ = val; break;
            case 6: bus_.write(hl(), val); break;
            case 7: a_ = val; break;
        }
    };

    int cycles = (reg == 6) ? 16 : 8;

    if (row == 1) {
        u8 val = get_reg();
        set_flag_z(((val >> bit) & 1) == 0);
        set_flag_n(false);
        set_flag_h(true);
        return cycles;
    }

    if (row == 2) {
        set_reg(get_reg() & ~static_cast<u8>(1 << bit));
        return cycles;
    }

    if (row == 3) {
        set_reg(get_reg() | static_cast<u8>(1 << bit));
        return cycles;
    }

    u8 val = get_reg();
    u8 result = 0;

    switch (bit) {
        case 0: {
            bool c = (val >> 7) & 1;
            result = static_cast<u8>((val << 1) | c);
            set_flags(result == 0, false, false, c);
            break;
        }
        case 1: {
            bool c = val & 1;
            result = static_cast<u8>((val >> 1) | (c ? 0x80 : 0));
            set_flags(result == 0, false, false, c);
            break;
        }
        case 2: {
            bool c = (val >> 7) & 1;
            result = static_cast<u8>((val << 1) | flag_c());
            set_flags(result == 0, false, false, c);
            break;
        }
        case 3: {
            bool c = val & 1;
            result = static_cast<u8>((val >> 1) | (flag_c() ? 0x80 : 0));
            set_flags(result == 0, false, false, c);
            break;
        }
        case 4: {
            bool c = (val >> 7) & 1;
            result = static_cast<u8>(val << 1);
            set_flags(result == 0, false, false, c);
            break;
        }
        case 5: {
            bool c = val & 1;
            result = static_cast<u8>((val >> 1) | (val & 0x80));
            set_flags(result == 0, false, false, c);
            break;
        }
        case 6: {
            result = static_cast<u8>((val << 4) | (val >> 4));
            set_flags(result == 0, false, false, false);
            break;
        }
        case 7: {
            bool c = val & 1;
            result = static_cast<u8>(val >> 1);
            set_flags(result == 0, false, false, c);
            break;
        }
    }

    set_reg(result);
    return cycles;
}

int CPU::execute(u8 op) {
    switch (op) {

    case 0x00: return 4;

    // ---- LOAD IMMEDIATE ----
    case 0x06: b_ = fetch8(); return 8;
    case 0x0E: c_ = fetch8(); return 8;
    case 0x16: d_ = fetch8(); return 8;
    case 0x1E: e_ = fetch8(); return 8;
    case 0x26: h_ = fetch8(); return 8;
    case 0x2E: l_ = fetch8(); return 8;
    case 0x3E: a_ = fetch8(); return 8;
    case 0x36: bus_.write(hl(), fetch8()); return 12;

    // ---- LOAD REGISTER TO REGISTER ----
    case 0x40: return 4;
    case 0x41: b_ = c_; return 4;
    case 0x42: b_ = d_; return 4;
    case 0x43: b_ = e_; return 4;
    case 0x44: b_ = h_; return 4;
    case 0x45: b_ = l_; return 4;
    case 0x46: b_ = bus_.read(hl()); return 8;
    case 0x47: b_ = a_; return 4;
    case 0x48: c_ = b_; return 4;
    case 0x49: return 4;
    case 0x4A: c_ = d_; return 4;
    case 0x4B: c_ = e_; return 4;
    case 0x4C: c_ = h_; return 4;
    case 0x4D: c_ = l_; return 4;
    case 0x4E: c_ = bus_.read(hl()); return 8;
    case 0x4F: c_ = a_; return 4;
    case 0x78: a_ = b_; return 4;
    case 0x79: a_ = c_; return 4;
    case 0x7A: a_ = d_; return 4;
    case 0x7B: a_ = e_; return 4;
    case 0x7C: a_ = h_; return 4;
    case 0x7D: a_ = l_; return 4;
    case 0x7E: a_ = bus_.read(hl()); return 8;
    case 0x7F: return 4;
    // Row D
    case 0x50: d_ = b_; return 4;
    case 0x51: d_ = c_; return 4;
    case 0x52: return 4;
    case 0x53: d_ = e_; return 4;
    case 0x54: d_ = h_; return 4;
    case 0x55: d_ = l_; return 4;
    case 0x56: d_ = bus_.read(hl()); return 8;
    case 0x57: d_ = a_; return 4;

    // Row E
    case 0x58: e_ = b_; return 4;
    case 0x59: e_ = c_; return 4;
    case 0x5A: e_ = d_; return 4;
    case 0x5B: return 4;
    case 0x5C: e_ = h_; return 4;
    case 0x5D: e_ = l_; return 4;
    case 0x5E: e_ = bus_.read(hl()); return 8;
    case 0x5F: e_ = a_; return 4;

    // Row H
    case 0x60: h_ = b_; return 4;
    case 0x61: h_ = c_; return 4;
    case 0x62: h_ = d_; return 4;
    case 0x63: h_ = e_; return 4;
    case 0x64: return 4;
    case 0x65: h_ = l_; return 4;
    case 0x66: h_ = bus_.read(hl()); return 8;
    case 0x67: h_ = a_; return 4;

    // Row L
    case 0x68: l_ = b_; return 4;
    case 0x69: l_ = c_; return 4;
    case 0x6A: l_ = d_; return 4;
    case 0x6B: l_ = e_; return 4;
    case 0x6C: l_ = h_; return 4;
    case 0x6D: return 4;
    case 0x6E: l_ = bus_.read(hl()); return 8;
    case 0x6F: l_ = a_; return 4;

    // Row (HL)
    case 0x70: bus_.write(hl(), b_); return 8;
    case 0x71: bus_.write(hl(), c_); return 8;
    case 0x72: bus_.write(hl(), d_); return 8;
    case 0x73: bus_.write(hl(), e_); return 8;
    case 0x74: bus_.write(hl(), h_); return 8;
    case 0x75: bus_.write(hl(), l_); return 8;

    // ---- LOAD A <-> MEMORY ----
    case 0x02: bus_.write(bc(), a_); return 8;
    case 0x12: bus_.write(de(), a_); return 8;
    case 0x77: bus_.write(hl(), a_); return 8;
    case 0x0A: a_ = bus_.read(bc()); return 8;
    case 0x1A: a_ = bus_.read(de()); return 8;
    case 0x22: bus_.write(hl(), a_); set_hl(hl() + 1); return 8;
    case 0x2A: a_ = bus_.read(hl()); set_hl(hl() + 1); return 8;
    case 0x32: bus_.write(hl(), a_); set_hl(hl() - 1); return 8;
    case 0x3A: a_ = bus_.read(hl()); set_hl(hl() - 1); return 8;
    case 0xFA: a_ = bus_.read(fetch16()); return 16;
    case 0xEA: bus_.write(fetch16(), a_); return 16;

    // ---- LOAD 16-BIT ----
    case 0x01: set_bc(fetch16()); return 12;
    case 0x11: set_de(fetch16()); return 12;
    case 0x21: set_hl(fetch16()); return 12;
    case 0x31: sp_ = fetch16(); return 12;

    // ---- STACK ----
    case 0xC5: push(bc()); return 16;
    case 0xD5: push(de()); return 16;
    case 0xE5: push(hl()); return 16;
    case 0xF5: push(af()); return 16;
    case 0xC1: set_bc(pop()); return 12;
    case 0xD1: set_de(pop()); return 12;
    case 0xE1: set_hl(pop()); return 12;
    case 0xF1: set_af(pop()); return 12;

    // ---- ARITHMETIC ----
    case 0x80: a_ = alu_add(a_, b_); return 4;
    case 0x81: a_ = alu_add(a_, c_); return 4;
    case 0x82: a_ = alu_add(a_, d_); return 4;
    case 0x83: a_ = alu_add(a_, e_); return 4;
    case 0x84: a_ = alu_add(a_, h_); return 4;
    case 0x85: a_ = alu_add(a_, l_); return 4;
    case 0x86: a_ = alu_add(a_, bus_.read(hl())); return 8;
    case 0x87: a_ = alu_add(a_, a_); return 4;
    case 0xC6: a_ = alu_add(a_, fetch8()); return 8;
    case 0x88: a_ = alu_add(a_, b_, flag_c()); return 4;
    case 0x89: a_ = alu_add(a_, c_, flag_c()); return 4;
    case 0x8A: a_ = alu_add(a_, d_, flag_c()); return 4;
    case 0x8B: a_ = alu_add(a_, e_, flag_c()); return 4;
    case 0x8C: a_ = alu_add(a_, h_, flag_c()); return 4;
    case 0x8D: a_ = alu_add(a_, l_, flag_c()); return 4;
    case 0x8E: a_ = alu_add(a_, bus_.read(hl()), flag_c()); return 8;
    case 0x8F: a_ = alu_add(a_, a_, flag_c()); return 4;
    case 0xCE: a_ = alu_add(a_, fetch8(), flag_c()); return 8;
    case 0x90: a_ = alu_sub(a_, b_); return 4;
    case 0x91: a_ = alu_sub(a_, c_); return 4;
    case 0x92: a_ = alu_sub(a_, d_); return 4;
    case 0x93: a_ = alu_sub(a_, e_); return 4;
    case 0x94: a_ = alu_sub(a_, h_); return 4;
    case 0x95: a_ = alu_sub(a_, l_); return 4;
    case 0x96: a_ = alu_sub(a_, bus_.read(hl())); return 8;
    case 0x97: a_ = alu_sub(a_, a_); return 4;
    case 0xD6: a_ = alu_sub(a_, fetch8()); return 8;
    case 0x98: a_ = alu_sub(a_, b_, flag_c()); return 4;
    case 0x99: a_ = alu_sub(a_, c_, flag_c()); return 4;
    case 0x9A: a_ = alu_sub(a_, d_, flag_c()); return 4;
    case 0x9B: a_ = alu_sub(a_, e_, flag_c()); return 4;
    case 0x9C: a_ = alu_sub(a_, h_, flag_c()); return 4;
    case 0x9D: a_ = alu_sub(a_, l_, flag_c()); return 4;
    case 0x9E: a_ = alu_sub(a_, bus_.read(hl()), flag_c()); return 8;
    case 0x9F: a_ = alu_sub(a_, a_, flag_c()); return 4;
    case 0xDE: a_ = alu_sub(a_, fetch8(), flag_c()); return 8;
    case 0x04: b_ = alu_inc(b_); return 4;
    case 0x0C: c_ = alu_inc(c_); return 4;
    case 0x14: d_ = alu_inc(d_); return 4;
    case 0x1C: e_ = alu_inc(e_); return 4;
    case 0x24: h_ = alu_inc(h_); return 4;
    case 0x2C: l_ = alu_inc(l_); return 4;
    case 0x3C: a_ = alu_inc(a_); return 4;
    case 0x34: { u8 t = alu_inc(bus_.read(hl())); bus_.write(hl(), t); return 12; }
    case 0x05: b_ = alu_dec(b_); return 4;
    case 0x0D: c_ = alu_dec(c_); return 4;
    case 0x15: d_ = alu_dec(d_); return 4;
    case 0x1D: e_ = alu_dec(e_); return 4;
    case 0x25: h_ = alu_dec(h_); return 4;
    case 0x2D: l_ = alu_dec(l_); return 4;
    case 0x3D: a_ = alu_dec(a_); return 4;
    case 0x35: { u8 t = alu_dec(bus_.read(hl())); bus_.write(hl(), t); return 12; }

    // ---- 16-BIT INC / DEC ----
    case 0x03: set_bc(bc() + 1); return 8;
    case 0x13: set_de(de() + 1); return 8;
    case 0x23: set_hl(hl() + 1); return 8;
    case 0x33: sp_++; return 8;
    case 0x0B: set_bc(bc() - 1); return 8;
    case 0x1B: set_de(de() - 1); return 8;
    case 0x2B: set_hl(hl() - 1); return 8;
    case 0x3B: sp_--; return 8;

    // ---- 16-BIT ADD HL ----
    case 0x09: { u32 r = hl() + bc(); set_flag_n(false); set_flag_h((hl() & 0xFFF) + (bc() & 0xFFF) > 0xFFF); set_flag_c(r > 0xFFFF); set_hl(static_cast<u16>(r)); return 8; }
    case 0x19: { u32 r = hl() + de(); set_flag_n(false); set_flag_h((hl() & 0xFFF) + (de() & 0xFFF) > 0xFFF); set_flag_c(r > 0xFFFF); set_hl(static_cast<u16>(r)); return 8; }
    case 0x29: { u32 r = hl() + hl(); set_flag_n(false); set_flag_h((hl() & 0xFFF) + (hl() & 0xFFF) > 0xFFF); set_flag_c(r > 0xFFFF); set_hl(static_cast<u16>(r)); return 8; }
    case 0x39: { u32 r = hl() + sp_; set_flag_n(false); set_flag_h((hl() & 0xFFF) + (sp_ & 0xFFF) > 0xFFF); set_flag_c(r > 0xFFFF); set_hl(static_cast<u16>(r)); return 8; }

    // ---- LOGICAL ----
    case 0xA0: a_ = alu_and(a_, b_); return 4;
    case 0xA1: a_ = alu_and(a_, c_); return 4;
    case 0xA2: a_ = alu_and(a_, d_); return 4;
    case 0xA3: a_ = alu_and(a_, e_); return 4;
    case 0xA4: a_ = alu_and(a_, h_); return 4;
    case 0xA5: a_ = alu_and(a_, l_); return 4;
    case 0xA6: a_ = alu_and(a_, bus_.read(hl())); return 8;
    case 0xA7: a_ = alu_and(a_, a_); return 4;
    case 0xA8: a_ = alu_xor(a_, b_); return 4;
    case 0xA9: a_ = alu_xor(a_, c_); return 4;
    case 0xAA: a_ = alu_xor(a_, d_); return 4;
    case 0xAB: a_ = alu_xor(a_, e_); return 4;
    case 0xAC: a_ = alu_xor(a_, h_); return 4;
    case 0xAD: a_ = alu_xor(a_, l_); return 4;
    case 0xAE: a_ = alu_xor(a_, bus_.read(hl())); return 8;
    case 0xAF: a_ = alu_xor(a_, a_); return 4;
    case 0xB0: a_ = alu_or(a_, b_); return 4;
    case 0xB1: a_ = alu_or(a_, c_); return 4;
    case 0xB2: a_ = alu_or(a_, d_); return 4;
    case 0xB3: a_ = alu_or(a_, e_); return 4;
    case 0xB4: a_ = alu_or(a_, h_); return 4;
    case 0xB5: a_ = alu_or(a_, l_); return 4;
    case 0xB6: a_ = alu_or(a_, bus_.read(hl())); return 8;
    case 0xB7: a_ = alu_or(a_, a_); return 4;
    case 0xB8: alu_sub(a_, b_); return 4;
    case 0xB9: alu_sub(a_, c_); return 4;
    case 0xBA: alu_sub(a_, d_); return 4;
    case 0xBB: alu_sub(a_, e_); return 4;
    case 0xBC: alu_sub(a_, h_); return 4;
    case 0xBD: alu_sub(a_, l_); return 4;
    case 0xBE: alu_sub(a_, bus_.read(hl())); return 8;
    case 0xBF: alu_sub(a_, a_); return 4;
    case 0xE6: a_ = alu_and(a_, fetch8()); return 8;
    case 0xF6: a_ = alu_or(a_, fetch8()); return 8;
    case 0xEE: a_ = alu_xor(a_, fetch8()); return 8;
    case 0xFE: alu_sub(a_, fetch8()); return 8;

    // ---- JUMPS ----
    case 0xC3: pc_ = fetch16(); return 16;
    case 0xE9: pc_ = hl(); return 4;
    case 0x18: { s8 e = static_cast<s8>(fetch8()); pc_ = static_cast<u16>(pc_ + e); return 12; }
    case 0x20: { s8 e = static_cast<s8>(fetch8()); if (!flag_z()) { pc_ = static_cast<u16>(pc_ + e); return 12; } return 8; }
    case 0x28: { s8 e = static_cast<s8>(fetch8()); if (flag_z())  { pc_ = static_cast<u16>(pc_ + e); return 12; } return 8; }
    case 0x30: { s8 e = static_cast<s8>(fetch8()); if (!flag_c()) { pc_ = static_cast<u16>(pc_ + e); return 12; } return 8; }
    case 0x38: { s8 e = static_cast<s8>(fetch8()); if (flag_c())  { pc_ = static_cast<u16>(pc_ + e); return 12; } return 8; }
    case 0xC2: { u16 addr = fetch16(); if (!flag_z()) { pc_ = addr; return 16; } return 12; }
    case 0xCA: { u16 addr = fetch16(); if (flag_z())  { pc_ = addr; return 16; } return 12; }
    case 0xD2: { u16 addr = fetch16(); if (!flag_c()) { pc_ = addr; return 16; } return 12; }
    case 0xDA: { u16 addr = fetch16(); if (flag_c())  { pc_ = addr; return 16; } return 12; }

    // ---- CALLS / RETURNS ----
    case 0xCD: { u16 addr = fetch16(); push(pc_); pc_ = addr; return 24; }
    case 0xC4: { u16 addr = fetch16(); if (!flag_z()) { push(pc_); pc_ = addr; return 24; } return 12; }
    case 0xCC: { u16 addr = fetch16(); if (flag_z())  { push(pc_); pc_ = addr; return 24; } return 12; }
    case 0xD4: { u16 addr = fetch16(); if (!flag_c()) { push(pc_); pc_ = addr; return 24; } return 12; }
    case 0xDC: { u16 addr = fetch16(); if (flag_c())  { push(pc_); pc_ = addr; return 24; } return 12; }
    case 0xC9: pc_ = pop(); return 16;
    case 0xD9: pc_ = pop(); ime_ = true; return 16;
    case 0xC0: { if (!flag_z()) { pc_ = pop(); return 20; } return 8; }
    case 0xC8: { if (flag_z())  { pc_ = pop(); return 20; } return 8; }
    case 0xD0: { if (!flag_c()) { pc_ = pop(); return 20; } return 8; }
    case 0xD8: { if (flag_c())  { pc_ = pop(); return 20; } return 8; }
    case 0xC7: push(pc_); pc_ = 0x0000; return 16;
    case 0xCF: push(pc_); pc_ = 0x0008; return 16;
    case 0xD7: push(pc_); pc_ = 0x0010; return 16;
    case 0xDF: push(pc_); pc_ = 0x0018; return 16;
    case 0xE7: push(pc_); pc_ = 0x0020; return 16;
    case 0xEF: push(pc_); pc_ = 0x0028; return 16;
    case 0xF7: push(pc_); pc_ = 0x0030; return 16;
    case 0xFF: push(pc_); pc_ = 0x0038; return 16;

    // ---- INTERRUPTS ----
    case 0xF3: ime_ = false; return 4;
    case 0xFB: ime_next_ = true; return 4;

    // ---- MISC ----
    case 0x76: halted_ = true; return 4;
    case 0x2F: a_ = ~a_; set_flag_n(true); set_flag_h(true); return 4;
    case 0x3F: set_flag_n(false); set_flag_h(false); set_flag_c(!flag_c()); return 4;
    case 0x37: set_flag_n(false); set_flag_h(false); set_flag_c(true); return 4;
    case 0x07: { bool c = (a_ >> 7) & 1; a_ = static_cast<u8>((a_ << 1) | c); set_flags(false, false, false, c); return 4; }
    case 0x0F: { bool c = a_ & 1; a_ = static_cast<u8>((a_ >> 1) | (c ? 0x80 : 0)); set_flags(false, false, false, c); return 4; }
    case 0x17: { bool c = (a_ >> 7) & 1; a_ = static_cast<u8>((a_ << 1) | flag_c()); set_flags(false, false, false, c); return 4; }
    case 0x1F: { bool c = a_ & 1; a_ = static_cast<u8>((a_ >> 1) | (flag_c() ? 0x80 : 0)); set_flags(false, false, false, c); return 4; }
    case 0x08: { u16 addr = fetch16(); bus_.write16(addr, sp_); return 20; }
    case 0xF8: { s8 e = static_cast<s8>(fetch8()); set_flag_z(false); set_flag_n(false); set_flag_h(((sp_ & 0xF) + (e & 0xF)) > 0xF); set_flag_c(((sp_ & 0xFF) + static_cast<u8>(e)) > 0xFF); set_hl(static_cast<u16>(sp_ + e)); return 12; }
    case 0xF9: sp_ = hl(); return 8;
    case 0xE0: bus_.write(static_cast<u16>(0xFF00 + fetch8()), a_); return 12;
    case 0xF0: a_ = bus_.read(static_cast<u16>(0xFF00 + fetch8())); return 12;
    case 0xE2: bus_.write(static_cast<u16>(0xFF00 + c_), a_); return 8;
    case 0xF2: a_ = bus_.read(static_cast<u16>(0xFF00 + c_)); return 8;
    case 0x27: {
        u8 correction = 0;
        if (!flag_n()) {
            if (flag_h() || (a_ & 0x0F) > 9) correction |= 0x06;
            if (flag_c() || a_ > 0x99) { correction |= 0x60; set_flag_c(true); }
            a_ += correction;
        } else {
            if (flag_h()) correction |= 0x06;
            if (flag_c()) correction |= 0x60;
            a_ -= correction;
        }
        set_flag_z(a_ == 0);
        set_flag_h(false);
        return 4;
    }
    case 0xCB: return execute_cb(fetch8());
    case 0x10: fetch8(); return 4; // STOP — we just skip it
    case 0xE8: { 
    s8 e = static_cast<s8>(fetch8()); 
    set_flag_z(false); 
    set_flag_n(false); 
    set_flag_h(((sp_ & 0xF) + (e & 0xF)) > 0xF); 
    set_flag_c(((sp_ & 0xFF) + static_cast<u8>(e)) > 0xFF); 
    sp_ = static_cast<u16>(sp_ + e); 
    return 16; 
}

    default:
        throw std::runtime_error(
            "Unimplemented opcode 0x" + 
            [op]{ char buf[5]; snprintf(buf, sizeof(buf), "%02X", op); return std::string(buf); }()
        );
    }
}

void CPU::dump_state() const {
    printf("AF=%04X BC=%04X DE=%04X HL=%04X SP=%04X PC=%04X Z=%d N=%d H=%d C=%d IME=%d\n",
        af(), bc(), de(), hl(), sp_, pc_,
        flag_z(), flag_n(), flag_h(), flag_c(), ime_);
}