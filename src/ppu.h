#pragma once

#include "types.h"
#include <array>
#include <SDL2/SDL.h>

// The Game Boy screen is 160x144 pixels
constexpr int SCREEN_WIDTH  = 160;
constexpr int SCREEN_HEIGHT = 144;

// PPU modes — the PPU cycles through these every scanline
// Mode 2: OAM scan        (80 T-cycles)
// Mode 3: Drawing pixels  (172 T-cycles)
// Mode 0: HBlank          (204 T-cycles)
// Mode 1: VBlank          (4560 T-cycles — 10 lines after line 143)
enum class PPUMode : u8 {
    HBlank = 0,
    VBlank = 1,
    OAMScan = 2,
    Drawing = 3,
};

class PPU {
public:
    PPU();
    ~PPU();

    // Called every CPU step with T-cycles elapsed.
    // Returns true when a full frame is ready to display.
    bool tick(int cycles);

    // Read/write PPU registers and VRAM/OAM
    u8   read(u16 address) const;
    void write(u16 address, u8 value);

    // Get pointer to the completed framebuffer (160x144 RGBA pixels)
    const std::array<u32, SCREEN_WIDTH * SCREEN_HEIGHT>& framebuffer() const {
        return framebuffer_;
    }

    // SDL window and renderer — public so main can present the frame
    SDL_Window*   window_   = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture*  texture_  = nullptr;

private:
    // VRAM — 8KB of tile and map data
    std::array<u8, 0x2000> vram_{};

    // OAM — 160 bytes of sprite data (40 sprites x 4 bytes each)
    std::array<u8, 0xA0> oam_{};

    // PPU registers
    u8 lcdc_ = 0x91; // LCD Control
    u8 stat_ = 0x00; // LCD Status
    u8 scy_  = 0x00; // Scroll Y
    u8 scx_  = 0x00; // Scroll X
    u8 ly_   = 0x00; // Current scanline (0-153)
    u8 lyc_  = 0x00; // LY Compare
    u8 bgp_  = 0xFC; // BG Palette
    u8 obp0_ = 0xFF; // Object Palette 0
    u8 obp1_ = 0xFF; // Object Palette 1
    u8 wy_   = 0x00; // Window Y
    u8 wx_   = 0x00; // Window X

    // Internal state
    PPUMode mode_         = PPUMode::OAMScan;
    int     cycle_count_  = 0;
    bool    frame_ready_  = false;

    // The completed pixel buffer — each u32 is ARGB
    std::array<u32, SCREEN_WIDTH * SCREEN_HEIGHT> framebuffer_{};

    // Game Boy 4-shade palette (dark green like the original screen)
    static constexpr u32 COLORS[4] = {
        0xFF9BBC0F, // White
        0xFF8BAC0F, // Light gray
        0xFF306230, // Dark gray
        0xFF0F380F, // Black
    };

    // Draw one scanline into the framebuffer
    void draw_scanline();
    void draw_background(int line);
    void draw_sprites(int line);
    void draw_window(int line);

    // Get color from palette register
    u32 get_color(u8 palette, u8 color_id) const;
};