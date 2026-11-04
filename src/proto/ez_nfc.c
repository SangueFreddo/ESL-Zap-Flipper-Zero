/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 */
#include "ez_nfc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EZ_NFC_ID_LEN 10

/* Character for each 6 bit value (value = position in the string). */
static const char ez_nfc_alphabet[] =
    "MOS8H57iPcvXWalBDxeKAws61mNy0f2RnZJ-tL3b9QpdETFqIu_zgrCUoY4hVjkG";

static int ez_nfc_value(char c) {
    const char* p = c ? strchr(ez_nfc_alphabet, c) : NULL;
    return p ? (int)(p - ez_nfc_alphabet) : -1;
}

/* Five characters, least significant first. */
static uint32_t ez_nfc_group(const char* s) {
    uint32_t v = 0;
    for(int i = 4; i >= 0; i--)
        v = v * 64U + (uint32_t)ez_nfc_value(s[i]);
    return v;
}

bool ez_nfc_decode_id(const char* id, char barcode[EZ_BARCODE_LEN + 1], bool* bad_check) {
    if(bad_check) *bad_check = false;
    if(!id || strlen(id) != EZ_NFC_ID_LEN) return false;
    for(int i = 0; i < EZ_NFC_ID_LEN; i++)
        if(ez_nfc_value(id[i]) < 0) return false;

    /* Second group carries the first 9 digits, first group the last 9. */
    uint32_t high = ez_nfc_group(id + 5);
    uint32_t low = ez_nfc_group(id);
    if(high > 999999999U || low > 999999999U) return false;

    char digits[19];
    snprintf(digits, sizeof(digits), "%09lu%09lu", (unsigned long)high, (unsigned long)low);
    int letter = (digits[0] - '0') * 10 + (digits[1] - '0');
    if(letter > 25) return false;

    barcode[0] = (char)('A' + letter);
    memcpy(barcode + 1, digits + 2, 16);
    barcode[EZ_BARCODE_LEN] = '\0';

    if(!ez_barcode_checksum_ok(barcode)) {
        if(bad_check) *bad_check = true;
        return false;
    }
    return true;
}

/* Collects the bytes of every "Page N: XX XX XX XX" line. */
static size_t ez_nfc_pages(const char* text, uint8_t* mem, size_t cap_pages) {
    size_t max_page = 0;
    bool any = false;
    const char* p = text;
    while(p && *p) {
        const char* line = p;
        const char* end = strchr(p, '\n');
        p = end ? end + 1 : NULL;
        if(strncmp(line, "Page ", 5) != 0) continue;
        char* after = NULL;
        long page = strtol(line + 5, &after, 10);
        if(!after || *after != ':' || page < 0 || (size_t)page >= cap_pages) continue;
        const char* q = after + 1;
        for(int b = 0; b < 4; b++) {
            while(*q == ' ')
                q++;
            char* stop = NULL;
            long v = strtol(q, &stop, 16);
            if(stop == q || v < 0 || v > 255) break;
            mem[page * 4 + b] = (uint8_t)v;
            q = stop;
        }
        any = true;
        if((size_t)page + 1 > max_page) max_page = (size_t)page + 1;
    }
    return any ? max_page : 0;
}

static const char* const ez_uri_prefixes[] = {
    "",
    "http://www.",
    "https://www.",
    "http://",
    "https://",
};

/* Finds the first NDEF URI record in the TLV area (starts at page 4). */
static bool ez_nfc_uri(const uint8_t* mem, size_t len, char* uri, size_t uri_size) {
    size_t off = 16;
    while(off < len) {
        uint8_t type = mem[off++];
        if(type == 0x00) continue; /* NULL TLV */
        if(type == 0xFE || off >= len) return false; /* terminator */
        size_t tlv_len = mem[off++];
        if(tlv_len == 0xFF) {
            if(off + 2 > len) return false;
            tlv_len = ((size_t)mem[off] << 8) | mem[off + 1];
            off += 2;
        }
        if(type != 0x03) {
            off += tlv_len;
            continue;
        }

        /* NDEF message: walk its records */
        size_t rec = off, msg_end = off + tlv_len;
        if(msg_end > len) msg_end = len;
        while(rec + 3 <= msg_end) {
            uint8_t header = mem[rec];
            bool short_rec = header & 0x10;
            bool has_id = header & 0x08;
            uint8_t tnf = header & 0x07;
            size_t p = rec + 1;
            uint8_t type_len = mem[p++];
            size_t payload_len;
            if(short_rec) {
                payload_len = mem[p++];
            } else {
                if(p + 4 > msg_end) return false;
                payload_len = ((size_t)mem[p] << 24) | ((size_t)mem[p + 1] << 16) |
                              ((size_t)mem[p + 2] << 8) | mem[p + 3];
                p += 4;
            }
            size_t id_len = has_id ? mem[p++] : 0;
            const uint8_t* rtype = &mem[p];
            p += type_len + id_len;
            if(p + payload_len > msg_end) return false;
            if(tnf == 0x01 && type_len == 1 && rtype[0] == 'U' && payload_len >= 1) {
                uint8_t code = mem[p];
                const char* prefix = code < 5 ? ez_uri_prefixes[code] : "";
                size_t n = 0;
                for(const char* c = prefix; *c && n + 1 < uri_size; c++)
                    uri[n++] = *c;
                for(size_t i = 1; i < payload_len && n + 1 < uri_size; i++)
                    uri[n++] = (char)mem[p + i];
                uri[n] = '\0';
                return true;
            }
            if(header & 0x40) break; /* message end */
            rec = p + payload_len;
        }
        return false;
    }
    return false;
}

static bool ez_contains_nocase(const char* hay, const char* needle) {
    size_t n = strlen(needle);
    for(; *hay; hay++) {
        size_t i = 0;
        while(i < n && hay[i] &&
              ((hay[i] | 0x20) == (needle[i] | 0x20)))
            i++;
        if(i == n) return true;
    }
    return false;
}

EzNfcResult ez_nfc_parse_memory(
    const uint8_t* mem,
    size_t len,
    char barcode[EZ_BARCODE_LEN + 1],
    char* uri,
    size_t uri_size) {
    char local_uri[160];
    if(!mem || len < 20) return EzNfcNotPages;
    if(!ez_nfc_uri(mem, len, local_uri, sizeof(local_uri))) return EzNfcNoNdef;
    if(uri && uri_size) {
        strncpy(uri, local_uri, uri_size - 1);
        uri[uri_size - 1] = '\0';
    }

    /* last path segment, without query or fragment */
    for(char* c = local_uri; *c; c++) {
        if(*c == '?' || *c == '#') {
            *c = '\0';
            break;
        }
    }
    size_t l = strlen(local_uri);
    while(l && local_uri[l - 1] == '/')
        local_uri[--l] = '\0';
    const char* seg = strrchr(local_uri, '/');
    seg = seg ? seg + 1 : local_uri;

    bool bad_check = false;
    if(ez_nfc_decode_id(seg, barcode, &bad_check)) return EzNfcOk;
    if(bad_check) return EzNfcBadCheck;
    if(ez_contains_nocase(local_uri, "imagotag") || ez_contains_nocase(local_uri, "vusion"))
        return EzNfcRadioTag;
    return EzNfcUnknownId;
}

EzNfcResult ez_nfc_parse(const char* text, char barcode[EZ_BARCODE_LEN + 1], char* uri, size_t uri_size) {
    uint8_t* mem = calloc(EZ_NFC_MAX_PAGES, 4);
    if(!mem) return EzNfcNotPages;
    size_t pages = ez_nfc_pages(text, mem, EZ_NFC_MAX_PAGES);
    EzNfcResult result =
        (pages < 5) ? EzNfcNotPages : ez_nfc_parse_memory(mem, pages * 4, barcode, uri, uri_size);
    free(mem);
    return result;
}
