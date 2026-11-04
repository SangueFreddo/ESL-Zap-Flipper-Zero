/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 */
#include "ez_encoder.h"

#include <stdlib.h>
#include <string.h>

/* Skips to row y0 of the source (sources only read forward). */
static bool ez_enc_seek_strip(EzEncoder* enc) {
    if(!ez_source_restart(enc->src)) return false;
    for(uint16_t y = 0; y < enc->y0; y++) {
        if(!ez_source_row(enc->src, enc->row)) return false;
    }
    enc->y = enc->y0;
    enc->x = 0;
    return ez_source_row(enc->src, enc->row);
}

bool ez_encoder_rewind(EzEncoder* enc) {
    enc->plane = 0;
    enc->started = false;
    enc->run_len = 0;
    enc->queue = 0;
    enc->queue_len = 0;
    enc->failed = false;
    enc->pixels_left = (uint32_t)enc->src->width * enc->rows * (enc->two_planes ? 2U : 1U);
    if(!ez_enc_seek_strip(enc)) {
        enc->failed = true;
        return false;
    }
    return true;
}

bool ez_encoder_init(
    EzEncoder* enc,
    EzSource* src,
    EzAccent accent,
    bool rle,
    uint16_t y0,
    uint16_t rows) {
    memset(enc, 0, sizeof(*enc));
    if(!src || rows == 0 || y0 + rows > src->height) return false;
    enc->src = src;
    enc->accent = accent;
    enc->two_planes = accent != EzAccentNone;
    enc->rle = rle;
    enc->y0 = y0;
    enc->rows = rows;
    enc->row = malloc(src->width);
    if(!enc->row) return false;
    return ez_encoder_rewind(enc);
}

void ez_encoder_deinit(EzEncoder* enc) {
    free(enc->row);
    enc->row = NULL;
}

/* Bit value of one pixel in the current plane. */
static inline uint8_t ez_enc_bit_of(const EzEncoder* enc, uint8_t px) {
    if(enc->plane == 0) {
        if(px == EZ_PX_ACCENT) {
            /* red accent is white in plane 1, yellow accent is black */
            return (enc->accent == EzAccentRed) ? 1U : 0U;
        }
        return (px == EZ_PX_WHITE) ? 1U : 0U;
    }
    return (px == EZ_PX_ACCENT) ? 0U : 1U;
}

/* Next pixel bit of the plane stream. Caller checks pixels_left first. */
static uint8_t ez_enc_next_pixel(EzEncoder* enc) {
    if(enc->x >= enc->src->width) {
        enc->x = 0;
        enc->y++;
        if(enc->y >= enc->y0 + enc->rows) {
            /* end of plane 1, start the accent plane */
            enc->plane = 1;
            if(!ez_enc_seek_strip(enc)) {
                enc->failed = true;
                return 1;
            }
        } else if(!ez_source_row(enc->src, enc->row)) {
            enc->failed = true;
            return 1;
        }
    }
    uint8_t bit = ez_enc_bit_of(enc, enc->row[enc->x]);
    enc->x++;
    enc->pixels_left--;
    return bit;
}

static void ez_enc_push(EzEncoder* enc, uint32_t value, uint8_t bits) {
    enc->queue = (enc->queue << bits) | (value & ((1ULL << bits) - 1U));
    enc->queue_len = (uint8_t)(enc->queue_len + bits);
}

/* Elias gamma: (n-1) zeros followed by the n significant bits. */
static void ez_enc_push_run(EzEncoder* enc, uint32_t run) {
    uint8_t n = 0;
    for(uint32_t v = run; v; v >>= 1)
        n++;
    ez_enc_push(enc, 0, (uint8_t)(n - 1U));
    ez_enc_push(enc, run, n);
}

/* Produces the next stream bit. Returns false when the stream is over. */
static bool ez_enc_next_bit(EzEncoder* enc, uint8_t* bit) {
    if(!enc->rle) {
        if(enc->pixels_left == 0) return false;
        *bit = ez_enc_next_pixel(enc);
        return true;
    }

    while(enc->queue_len == 0) {
        if(!enc->started) {
            if(enc->pixels_left == 0) return false;
            enc->run_value = ez_enc_next_pixel(enc);
            enc->run_len = 1;
            enc->started = true;
            ez_enc_push(enc, enc->run_value, 1);
            break;
        }
        if(enc->run_len == 0) return false; /* everything flushed */
        bool flushed = false;
        while(enc->pixels_left > 0) {
            uint8_t px = ez_enc_next_pixel(enc);
            if(px == enc->run_value) {
                enc->run_len++;
            } else {
                ez_enc_push_run(enc, enc->run_len);
                enc->run_value = px;
                enc->run_len = 1;
                flushed = true;
                break;
            }
        }
        if(!flushed) {
            ez_enc_push_run(enc, enc->run_len);
            enc->run_len = 0;
        }
    }

    enc->queue_len--;
    *bit = (uint8_t)((enc->queue >> enc->queue_len) & 1U);
    return true;
}

uint32_t ez_encoder_measure(EzEncoder* enc) {
    if(!enc->rle) {
        uint32_t bits = enc->pixels_left;
        return bits;
    }
    uint32_t bits = 0;
    uint8_t bit;
    while(ez_enc_next_bit(enc, &bit) && !enc->failed)
        bits++;
    ez_encoder_rewind(enc);
    return bits;
}

bool ez_encoder_chunk(EzEncoder* enc, uint8_t out[EZ_CHUNK_BYTES]) {
    for(uint8_t i = 0; i < EZ_CHUNK_BYTES; i++) {
        uint8_t byte = 0;
        for(uint8_t b = 0; b < 8; b++) {
            uint8_t bit = 0;
            if(!ez_enc_next_bit(enc, &bit)) bit = 0;
            byte = (uint8_t)((byte << 1) | bit);
        }
        out[i] = byte;
    }
    return !enc->failed;
}
