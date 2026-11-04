/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 *
 * Label import from NFC.
 *
 * Some labels carry an NFC chip (NTAG / Ultralight family) holding an NDEF
 * URI. Its last path segment is a 10 character id in a custom base64
 * alphabet that decodes to the full 17 character barcode. The id format was
 * documented by the TagTinker community (contribution by 7h30th3r0n3).
 *
 * This module is pure decoding: it works on the raw memory of the chip
 * (read live by app/ez_nfc_reader.c) or on a saved Flipper .nfc file.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "ez_proto.h"

typedef enum {
    EzNfcOk = 0,
    EzNfcNotPages, /* no "Page N:" data: not an NTAG / Ultralight dump */
    EzNfcNoNdef, /* no NDEF URI record */
    EzNfcRadioTag, /* URI points to the radio (VUSION) system */
    EzNfcUnknownId, /* URI found but the id is not a label id */
    EzNfcBadCheck, /* id decoded but the check digit is wrong */
} EzNfcResult;

#define EZ_NFC_MAX_PAGES 256

/* Decodes the raw chip memory (pages from 0, 4 bytes each). */
EzNfcResult ez_nfc_parse_memory(
    const uint8_t* mem,
    size_t len,
    char barcode[EZ_BARCODE_LEN + 1],
    char* uri,
    size_t uri_size);

/* Parses the text of a Flipper .nfc file. On success `barcode` holds the
 * 17 character barcode. `uri` (may be NULL) receives the URI found. */
EzNfcResult ez_nfc_parse(const char* text, char barcode[EZ_BARCODE_LEN + 1], char* uri, size_t uri_size);

/* Decodes a 10 character id. Returns false when it is not a label id or the
 * check digit does not match (then `bad_check` is set when known). */
bool ez_nfc_decode_id(const char* id, char barcode[EZ_BARCODE_LEN + 1], bool* bad_check);
