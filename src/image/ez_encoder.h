/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 *
 * Streaming image encoder.
 *
 * The label expects one bit per pixel (1 = white), row major, MSB first.
 * Colour labels get a second plane right after the first one, where
 * 0 marks accent pixels. The bit stream is sent raw or run-length coded
 * (first pixel value, then every run length as an Elias gamma code).
 *
 * Nothing is buffered: pixels are pulled from an EzSource on demand, so
 * even 800x480 panels fit in the Flipper's RAM.
 */
#pragma once

#include "ez_source.h"
#include "../proto/ez_proto.h"

typedef struct {
    EzSource* src;
    bool two_planes;
    EzAccent accent;
    bool rle;
    uint16_t y0; /* first row of the strip */
    uint16_t rows; /* strip height */

    /* pixel cursor */
    uint8_t* row;
    uint16_t x;
    uint16_t y; /* absolute row of `row` */
    uint8_t plane;
    uint32_t pixels_left;
    bool failed;

    /* run-length state */
    bool started;
    uint8_t run_value;
    uint32_t run_len;
    uint64_t queue;
    uint8_t queue_len;
} EzEncoder;

/* Prepares an encoder over rows [y0, y0 + rows) of `src`. */
bool ez_encoder_init(
    EzEncoder* enc,
    EzSource* src,
    EzAccent accent,
    bool rle,
    uint16_t y0,
    uint16_t rows);

void ez_encoder_deinit(EzEncoder* enc);

/* Rewinds to the first pixel. */
bool ez_encoder_rewind(EzEncoder* enc);

/* Number of meaningful bits the stream will contain. Rewinds afterwards. */
uint32_t ez_encoder_measure(EzEncoder* enc);

/* Fills the next EZ_CHUNK_BYTES bytes (zero padded at the end).
 * Returns false on a read error. */
bool ez_encoder_chunk(EzEncoder* enc, uint8_t out[EZ_CHUNK_BYTES]);

/* Bytes needed for `bits`, rounded up to whole data frames. */
static inline uint32_t ez_encoder_padded_bytes(uint32_t bits) {
    uint32_t chunk_bits = EZ_CHUNK_BYTES * 8U;
    return ((bits + chunk_bits - 1U) / chunk_bits) * EZ_CHUNK_BYTES;
}
