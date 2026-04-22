#include "ppu.h"
#include <stdexcept>

constexpr u32 PPU::COLORS[4];

PPU::PPU() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        throw std::runtime_error("SDL_Init failed");
    }

    window_ = SDL_CreateWindow(
        "Game Boy Emulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH * 3,   // 3x scale — 480x432 window
        SCREEN_HEIGHT * 3,
        SDL_WINDOW_SHOWN
    );

    renderer_ = SDL_CreateRenderer(window_, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    texture_ = SDL_CreateTexture(renderer_,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_WIDTH, SCREEN_HEIGHT);
}

PPU::~PPU() {
    if (texture_)  SDL_DestroyTexture(texture_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_)   SDL_DestroyWindow(window_);
    SDL_Quit();
}

bool PPU::tick(int cycles) {
    if (!(lcdc_ & 0x80)) {
        cycle_count_ += cycles;
        if (cycle_count_ >= 70224) {
            cycle_count_ -= 70224;
            framebuffer_.fill(COLORS[0]);
            return true;
        }
        return false;
    }

    frame_ready_ = false;
    cycle_count_ += cycles;

    switch (mode_) {
        case PPUMode::OAMScan:
            if (cycle_count_ >= 80) {
                cycle_count_ -= 80;
                mode_ = PPUMode::Drawing;
                stat_ = (stat_ & 0xFC) | 0x03;
            }
            break;

        case PPUMode::Drawing:
            if (cycle_count_ >= 172) {
                cycle_count_ -= 172;
                draw_scanline();
                mode_ = PPUMode::HBlank;
                stat_ = (stat_ & 0xFC) | 0x00;
                if (stat_ & 0x08) stat_interrupt_ = true;
            }
            break;

        case PPUMode::HBlank:
            if (cycle_count_ >= 204) {
                cycle_count_ -= 204;
                ly_++;

                // LYC coincidence check
                if (ly_ == lyc_) {
                    stat_ |= 0x04;
                    if (stat_ & 0x40) stat_interrupt_ = true;
                } else {
                    stat_ &= ~0x04;
                }

                if (ly_ == 144) {
                    mode_ = PPUMode::VBlank;
                    stat_ = (stat_ & 0xFC) | 0x01;
                    if (stat_ & 0x10) stat_interrupt_ = true;
                    frame_ready_ = true;
                } else {
                    mode_ = PPUMode::OAMScan;
                    stat_ = (stat_ & 0xFC) | 0x02;
                    if (stat_ & 0x20) stat_interrupt_ = true;
                }
            }
            break;

        case PPUMode::VBlank:
            if (cycle_count_ >= 456) {
                cycle_count_ -= 456;
                ly_++;
                if (ly_ > 153) {
                    ly_ = 0;
                    mode_ = PPUMode::OAMScan;
                    stat_ = (stat_ & 0xFC) | 0x02;
                    if (stat_ & 0x20) stat_interrupt_ = true;
                }
            }
            break;
    }

    return frame_ready_;
}
u8 PPU::read(u16 address) const {
    // VRAM: 0x8000-0x9FFF
    if (address >= 0x8000 && address < 0xA000) {
        return vram_[address - 0x8000];
    }
    // OAM: 0xFE00-0xFE9F
    if (address >= 0xFE00 && address < 0xFEA0) {
        return oam_[address - 0xFE00];
    }
    // Registers
    switch (address) {
        case 0xFF40: return lcdc_;
        case 0xFF41: return stat_;
        case 0xFF42: return scy_;
        case 0xFF43: return scx_;
        case 0xFF44: return ly_;
        case 0xFF45: return lyc_;
        case 0xFF47: return bgp_;
        case 0xFF48: return obp0_;
        case 0xFF49: return obp1_;
        case 0xFF4A: return wy_;
        case 0xFF4B: return wx_;
        default:     return 0xFF;
    }
}

void PPU::write(u16 address, u8 value) {
    if (address >= 0x8000 && address < 0xA000) {
        vram_[address - 0x8000] = value;
        return;
    }
    if (address >= 0xFE00 && address < 0xFEA0) {
        oam_[address - 0xFE00] = value;
        return;
    }
    switch (address) {
        case 0xFF40: lcdc_ = value; break;
        case 0xFF41: stat_ = value & 0xF8; break;
        case 0xFF42: scy_  = value; break;
        case 0xFF43: scx_  = value; break;
        case 0xFF44: ly_   = 0; break;    // Writing to LY resets it
        case 0xFF45: lyc_  = value; break;
        case 0xFF47: bgp_  = value; break;
        case 0xFF48: obp0_ = value; break;
        case 0xFF49: obp1_ = value; break;
        case 0xFF4A: wy_   = value; break;
        case 0xFF4B: wx_   = value; break;
        default: break;
    }
}

u32 PPU::get_color(u8 palette, u8 color_id) const {
    // --------------------------------------------------------
    // C++ LESSON: Bit manipulation for palette decoding
    // --------------------------------------------------------
    // The palette register packs 4 color mappings into one byte.
    // Each color ID (0-3) maps to 2 bits in the palette register.
    //
    // Palette byte layout:
    //   Bits 7-6: color for ID 3
    //   Bits 5-4: color for ID 2
    //   Bits 3-2: color for ID 1
    //   Bits 1-0: color for ID 0
    //
    // To extract color for a given ID:
    //   1. Shift right by (color_id * 2) to move target bits to position 0-1
    //   2. AND with 0x03 to isolate just those 2 bits
    //   3. Use result (0-3) to index into COLORS array
    u8 shade = (palette >> (color_id * 2)) & 0x03;
    return COLORS[shade];
}
void PPU::draw_scanline() {
    if (ly_ >= SCREEN_HEIGHT) return;
    draw_background(ly_);
    draw_window(ly_);
    draw_sprites(ly_);
}

void PPU::draw_background(int line) {
    // --------------------------------------------------------
    // C++ LESSON: Pointer arithmetic and array indexing
    // --------------------------------------------------------
    // framebuffer_ is a flat 1D array representing a 2D screen.
    // To get the pixel at (x, y): framebuffer_[y * SCREEN_WIDTH + x]
    // This is how 2D grids are stored in memory in C++ —
    // row by row, one after another.

    bool bg_enabled = (lcdc_ & 0x01) != 0;
    if (!bg_enabled) {
        // Fill line with white
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            framebuffer_[line * SCREEN_WIDTH + x] = COLORS[0];
        }
        return;
    }

    // Which tile map to use: bit 3 of LCDC selects 0x9800 or 0x9C00
    u16 map_base = (lcdc_ & 0x08) ? 0x9C00 : 0x9800;

    // Which tile data table: bit 4 of LCDC
    // 0 = 0x8800 with signed tile IDs, 1 = 0x8000 with unsigned
    bool signed_tiles = !(lcdc_ & 0x10);

    // The background is a 256x256 pixel space that wraps around.
    // SCY and SCX scroll the 160x144 viewport across it.
    int scroll_y = (line + scy_) & 0xFF;
    int tile_row = scroll_y / 8; // Which row of tiles

    for (int x = 0; x < SCREEN_WIDTH; x++) {
        int scroll_x  = (x + scx_) & 0xFF;
        int tile_col  = scroll_x / 8;

        // Look up tile ID in the map
        u16 map_addr  = map_base + tile_row * 32 + tile_col;
        u8  tile_id   = vram_[map_addr - 0x8000];

        // Find tile data address
        u16 tile_data_addr;
        if (signed_tiles) {
            // Signed addressing: tile 0 is at 0x9000
            s8 signed_id = static_cast<s8>(tile_id);
            tile_data_addr = static_cast<u16>(0x9000 + signed_id * 16);
        } else {
            // Unsigned addressing: tile 0 is at 0x8000
            tile_data_addr = static_cast<u16>(0x8000 + tile_id * 16);
        }

        // Each tile is 8x8 pixels, 2 bits per pixel, 2 bytes per row
        int pixel_y = scroll_y % 8;
        u16 row_addr = tile_data_addr + pixel_y * 2;

        // --------------------------------------------------------
        // C++ LESSON: 2bpp tile decoding — the core of GB graphics
        // --------------------------------------------------------
        // Each row of a tile is stored in 2 bytes.
        // Byte 1 holds the low bit of each pixel's color ID.
        // Byte 2 holds the high bit.
        // Pixel 0 is in bit 7 (leftmost), pixel 7 is in bit 0.
        //
        // To get the color ID for pixel at position p:
        //   low  bit = (byte1 >> (7 - p)) & 1
        //   high bit = (byte2 >> (7 - p)) & 1
        //   color_id = (high << 1) | low

        u8 byte1 = vram_[row_addr - 0x8000];
        u8 byte2 = vram_[row_addr - 0x8000 + 1];

        int pixel_x  = scroll_x % 8;
        int bit      = 7 - pixel_x;
        u8  low      = (byte1 >> bit) & 1;
        u8  high     = (byte2 >> bit) & 1;
        u8  color_id = static_cast<u8>((high << 1) | low);

        framebuffer_[line * SCREEN_WIDTH + x] = get_color(bgp_, color_id);
    }
}
void PPU::draw_window(int line) {
    if (!(lcdc_ & 0x20)) return; // Window disabled
    if (line < wy_) return;      // Current line is above window

    int wx_adjusted = wx_ - 7;   // WX is offset by 7 on real hardware
    if (wx_adjusted >= SCREEN_WIDTH) return;

    u16 map_base = (lcdc_ & 0x40) ? 0x9C00 : 0x9800;
    bool signed_tiles = !(lcdc_ & 0x10);

    int window_line = line - wy_;
    int tile_row = window_line / 8;

    for (int x = wx_adjusted; x < SCREEN_WIDTH; x++) {
        int window_x = x - wx_adjusted;
        int tile_col = window_x / 8;

        u16 map_addr = map_base + tile_row * 32 + tile_col;
        u8  tile_id  = vram_[map_addr - 0x8000];

        u16 tile_data_addr;
        if (signed_tiles) {
            s8 signed_id = static_cast<s8>(tile_id);
            tile_data_addr = static_cast<u16>(0x9000 + signed_id * 16);
        } else {
            tile_data_addr = static_cast<u16>(0x8000 + tile_id * 16);
        }

        int pixel_y  = window_line % 8;
        u16 row_addr = tile_data_addr + pixel_y * 2;

        u8 byte1 = vram_[row_addr - 0x8000];
        u8 byte2 = vram_[row_addr - 0x8000 + 1];

        int pixel_x = window_x % 8;
        int bit     = 7 - pixel_x;
        u8  low     = (byte1 >> bit) & 1;
        u8  high    = (byte2 >> bit) & 1;
        u8  color_id = static_cast<u8>((high << 1) | low);

        framebuffer_[line * SCREEN_WIDTH + x] = get_color(bgp_, color_id);
    }
}

void PPU::draw_sprites(int line) {
    if (!(lcdc_ & 0x02)) return; // Sprites disabled

    // Sprite height: bit 2 of LCDC selects 8x8 or 8x16
    int sprite_height = (lcdc_ & 0x04) ? 16 : 8;
    int sprites_drawn = 0;

    // OAM has 40 sprites, 4 bytes each:
    // Byte 0: Y position (minus 16)
    // Byte 1: X position (minus 8)
    // Byte 2: Tile index
    // Byte 3: Flags (palette, flip, priority)
    for (int i = 0; i < 40 && sprites_drawn < 10; i++) {
        int base  = i * 4;
        int spr_y = oam_[base + 0] - 16;
        int spr_x = oam_[base + 1] - 8;
        u8  tile  = oam_[base + 2];
        u8  flags = oam_[base + 3];

        if (line < spr_y || line >= spr_y + sprite_height) continue;

        sprites_drawn++;

        bool flip_y   = (flags & 0x40) != 0;
        bool flip_x   = (flags & 0x20) != 0;
        u8   palette  = (flags & 0x10) ? obp1_ : obp0_;

        int tile_line = line - spr_y;
        if (flip_y) tile_line = sprite_height - 1 - tile_line;

        u16 tile_addr = static_cast<u16>(0x8000 + tile * 16 + tile_line * 2);
        u8  byte1     = vram_[tile_addr - 0x8000];
        u8  byte2     = vram_[tile_addr - 0x8000 + 1];

        for (int px = 0; px < 8; px++) {
            int screen_x = spr_x + px;
            if (screen_x < 0 || screen_x >= SCREEN_WIDTH) continue;

            int bit      = flip_x ? px : (7 - px);
            u8  low      = (byte1 >> bit) & 1;
            u8  high     = (byte2 >> bit) & 1;
            u8  color_id = static_cast<u8>((high << 1) | low);

            // Color ID 0 is transparent for sprites
            if (color_id == 0) continue;

            framebuffer_[line * SCREEN_WIDTH + screen_x] = get_color(palette, color_id);
        }
    }
}