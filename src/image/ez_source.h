/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 *
 * Pixel sources. A source produces an image row by row, top to bottom,
 * at a requested size. Pixels are EZ_PX_BLACK, EZ_PX_WHITE or EZ_PX_ACCENT.
 * Sources can be restarted, which the encoder needs because the label
 * receives the black/white plane first and the accent plane afterwards.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <storage/storage.h>

#include "../proto/ez_models.h"

#define EZ_PX_BLACK 0
#define EZ_PX_WHITE 1
#define EZ_PX_ACCENT 2

#define EZ_TEXT_MAX 64

typedef enum {
    EzSourceBmp = 0,
    EzSourceText,
    EzSourceTestCard,
} EzSourceKind;

typedef enum {
    EzFitContain = 0, /* whole picture visible, white bars */
    EzFitCover, /* fill the label, crop the overflow */
    EzFitStretch, /* fill the label, distort */
    EzFitCount,
} EzFit;

typedef struct {
    EzSourceKind kind;
    char path[256]; /* BMP file */
    char text[EZ_TEXT_MAX + 1]; /* text / test card caption */
    EzFit fit;
    bool dither;
    bool invert;
    bool frame; /* draw a border (text mode) */
    EzAccent accent; /* label accent colour, EzAccentNone disables it */
    bool accent_text; /* text mode: draw glyphs in the accent colour */
} EzRenderSpec;

typedef struct EzSource EzSource;

struct EzSource {
    uint16_t width;
    uint16_t height;
    bool (*restart)(EzSource* src);
    /* Writes `width` pixels. Rows must be read in order. */
    bool (*read_row)(EzSource* src, uint8_t* row);
    void (*destroy)(EzSource* src);
    void* ctx;
};

/* Creates a source rendering `spec` at width x height. NULL on failure,
 * with a short reason in `error` (may be NULL). */
EzSource* ez_source_create(
    const EzRenderSpec* spec,
    uint16_t width,
    uint16_t height,
    Storage* storage,
    const char** error);

/* Wraps a source of size (h x w) and returns it rotated 90 degrees so the
 * result is (w x h) as the label expects. Takes ownership of `inner`. */
EzSource* ez_source_rotate(EzSource* inner);

static inline bool ez_source_restart(EzSource* s) {
    return s->restart(s);
}
static inline bool ez_source_row(EzSource* s, uint8_t* row) {
    return s->read_row(s, row);
}
static inline void ez_source_free(EzSource* s) {
    if(s) s->destroy(s);
}

/* Reads the size of a BMP file without decoding it. */
bool ez_bmp_probe(Storage* storage, const char* path, uint16_t* w, uint16_t* h, uint16_t* bpp);
