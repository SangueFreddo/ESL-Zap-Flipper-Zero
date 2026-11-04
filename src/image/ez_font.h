/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#define EZ_FONT_W 5
#define EZ_FONT_H 7
#define EZ_FONT_FIRST 32
#define EZ_FONT_GLYPHS 95

extern const uint8_t ez_font5x7[EZ_FONT_GLYPHS][7];

/* True when column `col` of row `row` of character `c` is inked. */
static inline bool ez_font_pixel(char c, uint8_t col, uint8_t row) {
    uint8_t idx = (uint8_t)c;
    if(idx < EZ_FONT_FIRST || idx >= EZ_FONT_FIRST + EZ_FONT_GLYPHS) idx = '?';
    return (ez_font5x7[idx - EZ_FONT_FIRST][row] >> (EZ_FONT_W - 1 - col)) & 1U;
}
