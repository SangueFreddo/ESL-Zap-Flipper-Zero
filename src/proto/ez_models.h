/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 *
 * Known label models, looked up by the 4 digit model code printed in
 * the barcode.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    EzAccentNone = 0,
    EzAccentRed,
    EzAccentYellow,
} EzAccent;

typedef enum {
    EzKindUnknown = 0,
    EzKindGraphic,
    EzKindSegment,
} EzKind;

/* The panel is mounted rotated: the wire image is height x width. */
#define EZ_MODEL_ROTATED (1U << 0)
/* Page 0/1 are reserved by the store system, images go to page 2+. */
#define EZ_MODEL_PAGE2 (1U << 1)

typedef struct {
    uint16_t code;
    uint16_t width; /* visible panel width */
    uint16_t height; /* visible panel height */
    EzKind kind;
    EzAccent accent;
    uint8_t flags;
    const char* name;
} EzModel;

/* Returns NULL for unknown codes. */
const EzModel* ez_model_find(uint16_t code);

/* Builds a model for a label whose code is not in the table. */
void ez_model_custom(EzModel* out, uint16_t code, uint16_t w, uint16_t h, EzAccent accent);

const char* ez_accent_name(EzAccent accent);

/* Common panel sizes offered for unknown labels. */
typedef struct {
    uint16_t w;
    uint16_t h;
} EzSize;

extern const EzSize ez_common_sizes[];
extern const uint8_t ez_common_sizes_count;
