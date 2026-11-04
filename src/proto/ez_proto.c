/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 */
#include "ez_proto.h"

#include <string.h>

/* ------------------------------------------------------------------------ */
/* Barcode                                                                  */
/* ------------------------------------------------------------------------ */

static bool ez_is_digit(char c) {
    return c >= '0' && c <= '9';
}

/*
 * Only digits 3..16 (index 2..15) end up in the infrared frames:
 * 3..12 form the address, 13..16 the model. The prefix (letter + digit) and
 * the final check digit are optional and may be EZ_BARCODE_UNKNOWN.
 */
bool ez_barcode_is_valid(const char* barcode) {
    if(!barcode || strlen(barcode) != EZ_BARCODE_LEN) return false;
    char c0 = barcode[0];
    if(!((c0 >= 'A' && c0 <= 'Z') || (c0 >= 'a' && c0 <= 'z') || c0 == EZ_BARCODE_UNKNOWN))
        return false;
    if(!ez_is_digit(barcode[1]) && barcode[1] != EZ_BARCODE_UNKNOWN) return false;
    for(size_t i = 2; i < 16; i++) {
        if(!ez_is_digit(barcode[i])) return false;
    }
    if(!ez_is_digit(barcode[16]) && barcode[16] != EZ_BARCODE_UNKNOWN) return false;
    return true;
}

bool ez_barcode_is_complete(const char* barcode) {
    return ez_barcode_is_valid(barcode) && barcode[0] != EZ_BARCODE_UNKNOWN &&
           barcode[1] != EZ_BARCODE_UNKNOWN && barcode[16] != EZ_BARCODE_UNKNOWN;
}

bool ez_barcode_checksum_ok(const char* barcode) {
    if(!ez_barcode_is_complete(barcode)) return false;
    /* sum of the ASCII codes of the first 16 characters, modulo 10 */
    uint32_t sum = 0;
    for(size_t i = 0; i < 16; i++) {
        char c = barcode[i];
        if(c >= 'a' && c <= 'z') c = (char)(c - 32);
        sum += (uint8_t)c;
    }
    return (char)('0' + (sum % 10U)) == barcode[16];
}

bool ez_barcode_same_label(const char* a, const char* b) {
    if(!ez_barcode_is_valid(a) || !ez_barcode_is_valid(b)) return false;
    return memcmp(a + 2, b + 2, 14) == 0;
}

static uint32_t ez_digits(const char* s, size_t from, size_t count) {
    uint32_t v = 0;
    for(size_t i = 0; i < count; i++) {
        v = v * 10U + (uint32_t)(s[from + i] - '0');
    }
    return v;
}

bool ez_barcode_address(const char* barcode, uint8_t address[4]) {
    if(!ez_barcode_is_valid(barcode)) return false;
    /* Two five digit groups combined as low + (high << 16). A low group
     * above 65535 carries into the high half, which is intended. */
    uint32_t id = ez_digits(barcode, 2, 5) + (ez_digits(barcode, 7, 5) << 16);
    address[0] = (uint8_t)((id >> 16) & 0xFFU);
    address[1] = (uint8_t)((id >> 24) & 0xFFU);
    address[2] = (uint8_t)(id & 0xFFU);
    address[3] = (uint8_t)((id >> 8) & 0xFFU);
    return true;
}

uint16_t ez_barcode_model(const char* barcode) {
    if(!ez_barcode_is_valid(barcode)) return 0;
    return (uint16_t)ez_digits(barcode, 12, 4);
}

/* ------------------------------------------------------------------------ */
/* Frames                                                                   */
/* ------------------------------------------------------------------------ */

uint16_t ez_crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0x8408U;
    while(len--) {
        crc ^= *data++;
        for(uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 1U) ? (uint16_t)((crc >> 1) ^ 0x8408U) : (uint16_t)(crc >> 1);
        }
    }
    return crc;
}

static void put8(EzFrame* f, uint8_t v) {
    if(f->len < EZ_FRAME_CAP) f->data[f->len++] = v;
}

static void put16be(EzFrame* f, uint16_t v) {
    put8(f, (uint8_t)(v >> 8));
    put8(f, (uint8_t)(v & 0xFFU));
}

static void fill(EzFrame* f, uint8_t v, uint8_t count) {
    while(count--)
        put8(f, v);
}

static void begin(EzFrame* f, uint8_t proto, const uint8_t address[4], uint8_t command) {
    f->len = 0;
    put8(f, proto);
    for(uint8_t i = 0; i < 4; i++)
        put8(f, address[i]);
    put8(f, command);
}

/* Graphic labels route image commands through command 0x34 ("to MCU"). */
static void begin_mcu(EzFrame* f, const uint8_t address[4], uint8_t sub_command) {
    begin(f, EZ_PROTO_GRAPHIC, address, 0x34);
    fill(f, 0x00, 3);
    put8(f, sub_command);
}

static void finish(EzFrame* f) {
    uint16_t crc = ez_crc16(f->data, f->len);
    put8(f, (uint8_t)(crc & 0xFFU));
    put8(f, (uint8_t)(crc >> 8));
}

void ez_frame_wake(EzFrame* f, const uint8_t address[4]) {
    begin(f, EZ_PROTO_GRAPHIC, address, 0x17);
    put8(f, 0x01);
    fill(f, 0x00, 3);
    fill(f, 0x01, 22);
    finish(f);
}

void ez_frame_image_header(EzFrame* f, const uint8_t address[4], const EzImageHeader* h) {
    begin_mcu(f, address, 0x05);
    put16be(f, h->byte_count);
    put8(f, 0x00);
    put8(f, (uint8_t)h->compression);
    put8(f, h->page);
    put16be(f, h->width);
    put16be(f, h->height);
    put16be(f, h->x);
    put16be(f, h->y);
    put16be(f, 0x0000); /* key code */
    put8(f, 0x88); /* update now + make this the base page */
    put16be(f, 0x0000); /* page mask */
    fill(f, 0x00, 4);
    finish(f);
}

void ez_frame_image_chunk(
    EzFrame* f,
    const uint8_t address[4],
    uint16_t index,
    const uint8_t chunk[EZ_CHUNK_BYTES]) {
    begin_mcu(f, address, 0x20);
    put16be(f, index);
    for(uint8_t i = 0; i < EZ_CHUNK_BYTES; i++)
        put8(f, chunk[i]);
    finish(f);
}

void ez_frame_refresh(EzFrame* f, const uint8_t address[4]) {
    begin_mcu(f, address, 0x01);
    fill(f, 0x00, 22);
    finish(f);
}

void ez_frame_show_page(EzFrame* f, const uint8_t address[4], uint8_t page, uint16_t seconds) {
    begin(f, EZ_PROTO_GRAPHIC, address, 0x06);
    uint8_t flags = (uint8_t)(((page & 7U) << 3) | 0x01U);
    if(seconds == 0) flags |= 0x80U; /* forever */
    put8(f, flags);
    fill(f, 0x00, 2);
    put16be(f, seconds);
    finish(f);
}

void ez_frame_show_page_segment(
    EzFrame* f,
    const uint8_t address[4],
    uint8_t page,
    uint16_t seconds) {
    begin(f, EZ_PROTO_SEGMENT, address, 0xAB);
    uint8_t duration;
    if(seconds == 0)
        duration = 0x80U;
    else if(seconds <= 2)
        duration = 1;
    else if(seconds <= 15)
        duration = 3;
    else
        duration = 5;
    put8(f, (uint8_t)(((page & 7U) << 3) | duration));
    fill(f, 0x00, 2);
    finish(f);
}
