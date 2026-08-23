#pragma once

// ─────────────────────────────────────────────────────────────
//  RAONE  –  OLED 128×64 monochrome layout tokens
//  All "colours" are boolean: true = white pixel, false = black
//  The display is used in normal mode (white-on-black).
//  "Inverted" rows are drawn with fillRect(black bg) + white text.
// ─────────────────────────────────────────────────────────────

// Display geometry
static constexpr uint8_t OLED_W           = 128;
static constexpr uint8_t OLED_H           = 64;

// Status-bar (top row)
static constexpr uint8_t UI_STATUSBAR_H   = 11;   // px tall
static constexpr uint8_t UI_STATUSBAR_Y   = 0;

// Separator line after status-bar
static constexpr uint8_t UI_SEP_Y         = UI_STATUSBAR_H;  // y = 11

// Content area
static constexpr uint8_t UI_CONTENT_Y     = UI_SEP_Y + 1;    // y = 12
static constexpr uint8_t UI_CONTENT_H     = OLED_H - UI_CONTENT_Y - 9; // leaves footer

// Footer hint bar (bottom)
static constexpr uint8_t UI_FOOTER_H      = 9;
static constexpr uint8_t UI_FOOTER_Y      = OLED_H - UI_FOOTER_H;      // y = 55

// Padding
static constexpr uint8_t UI_PAD           = 2;

// Menu row height (default 5x7 font = 8px per row, +1 gap)
static constexpr uint8_t UI_MENU_ROW_H    = 10;
static constexpr uint8_t UI_MENU_VISIBLE  = 4;    // rows visible in content area

// Text row heights for default 5×7 font
static constexpr uint8_t UI_LINE_H        = 9;    // line height with 1px gap

// Selection indicator character
#define UI_SEL_CHAR   ">"
#define UI_BLANK_CHAR " "