/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 *
 * Live NFC reader, in two phases like the Flipper's own NFC app:
 *   1. scan: waits (as long as needed) until any NFC chip answers, then
 *      tells whether it belongs to the NTAG / Ultralight family used by labels;
 *   2. read: copies the chip memory.
 * Callbacks come from NFC worker threads; the caller switches phases from its
 * own thread (never from inside a callback). Decoding is in proto/ez_nfc.c.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    EzNfcReaderFound, /* a label-type chip is present: start the read phase */
    EzNfcReaderIncompatible, /* a chip answered, but of another family */
    EzNfcReaderRead, /* memory read, available with ez_nfc_reader_memory */
    EzNfcReaderFailed, /* the read phase lost the chip or got an error */
} EzNfcReaderEvent;

typedef void (*EzNfcReaderCallback)(EzNfcReaderEvent event, void* context);

typedef struct EzNfcReader EzNfcReader;

EzNfcReader* ez_nfc_reader_alloc(EzNfcReaderCallback callback, void* context);
void ez_nfc_reader_free(EzNfcReader* reader);

/* Phase 1: search for a chip. Reports Found or Incompatible, once. */
void ez_nfc_reader_scan(EzNfcReader* reader);

/* Phase 2: read the chip found. Reports Read or Failed, once. */
void ez_nfc_reader_read(EzNfcReader* reader);

/* Stops the current phase. Never call it from the callback. */
void ez_nfc_reader_stop(EzNfcReader* reader);

/* Memory of the last chip read (pages from 0, 4 bytes each). */
const uint8_t* ez_nfc_reader_memory(EzNfcReader* reader, size_t* len);
