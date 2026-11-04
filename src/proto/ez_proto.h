/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 *
 * Wire format of infrared electronic shelf labels.
 *
 * Every frame looks like:
 *   [protocol] [address x4] [command] [payload ...] [crc lo] [crc hi]
 *
 * The address is derived from the 17 character label barcode
 * (one letter followed by 16 digits). The CRC is a reflected CRC-16
 * with polynomial 0x8408 and initial value 0x8408.
 *
 * Protocol facts come from public reverse engineering research
 * published by furrtek (furrtek.org, PrecIR).
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define EZ_PROTO_GRAPHIC 0x85 /* dot matrix / e-paper labels */
#define EZ_PROTO_SEGMENT 0x84 /* segment (price digit) labels */

#define EZ_BARCODE_LEN 17
#define EZ_FRAME_CAP 48
#define EZ_CHUNK_BYTES 20 /* image bytes carried by one data frame */

typedef struct {
    uint8_t data[EZ_FRAME_CAP];
    uint8_t len;
} EzFrame;

typedef enum {
    EzCompressionRaw = 0,
    EzCompressionRle = 2,
} EzCompression;

typedef struct {
    uint16_t byte_count; /* total payload bytes, multiple of EZ_CHUNK_BYTES */
    EzCompression compression;
    uint8_t page;
    uint16_t width;
    uint16_t height;
    uint16_t x;
    uint16_t y;
} EzImageHeader;

/* --- barcode helpers --------------------------------------------------- */

/* Placeholder for the optional characters (prefix and check digit). */
#define EZ_BARCODE_UNKNOWN '-'

/* True for "letter + 16 digits". The letter, the second digit and the
 * last (check) digit may be EZ_BARCODE_UNKNOWN: only digits 3..16 matter. */
bool ez_barcode_is_valid(const char* barcode);

/* True when the optional characters are present too. */
bool ez_barcode_is_complete(const char* barcode);

/* Verifies the check digit of a complete barcode. */
bool ez_barcode_checksum_ok(const char* barcode);

/* Same physical label (digits 3..16 equal). */
bool ez_barcode_same_label(const char* a, const char* b);

/* 32 bit label address in wire order. */
bool ez_barcode_address(const char* barcode, uint8_t address[4]);

/* Four digit model code (barcode digits 13..16). 0 when invalid. */
uint16_t ez_barcode_model(const char* barcode);

/* --- frame building ---------------------------------------------------- */

uint16_t ez_crc16(const uint8_t* data, size_t len);

/* Wake-up frame, repeated for a few seconds so a sleeping label hears it. */
void ez_frame_wake(EzFrame* f, const uint8_t address[4]);

/* Announces an image transfer: size, compression, geometry and page. */
void ez_frame_image_header(EzFrame* f, const uint8_t address[4], const EzImageHeader* h);

/* One block of EZ_CHUNK_BYTES image bytes. */
void ez_frame_image_chunk(
    EzFrame* f,
    const uint8_t address[4],
    uint16_t index,
    const uint8_t chunk[EZ_CHUNK_BYTES]);

/* Ends a transfer and asks the label to redraw the screen. */
void ez_frame_refresh(EzFrame* f, const uint8_t address[4]);

/* Shows a stored page. seconds = 0 means "keep it". */
void ez_frame_show_page(EzFrame* f, const uint8_t address[4], uint8_t page, uint16_t seconds);

/* Same for segment labels (coarse durations). */
void ez_frame_show_page_segment(
    EzFrame* f,
    const uint8_t address[4],
    uint8_t page,
    uint16_t seconds);
