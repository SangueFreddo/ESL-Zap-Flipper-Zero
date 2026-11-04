/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 */
#include "ez_nfc_reader.h"
#include "../proto/ez_nfc.h"

#include <furi.h>
#include <nfc/nfc.h>
#include <nfc/nfc_scanner.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller.h>

struct EzNfcReader {
    Nfc* nfc;
    NfcScanner* scanner;
    NfcPoller* poller;
    EzNfcReaderCallback callback;
    void* context;
    uint8_t memory[EZ_NFC_MAX_PAGES * 4];
    size_t length;
    volatile bool reported;
};

EzNfcReader* ez_nfc_reader_alloc(EzNfcReaderCallback callback, void* context) {
    EzNfcReader* reader = malloc(sizeof(EzNfcReader));
    memset(reader, 0, sizeof(EzNfcReader));
    reader->nfc = nfc_alloc();
    reader->callback = callback;
    reader->context = context;
    return reader;
}

void ez_nfc_reader_free(EzNfcReader* reader) {
    if(!reader) return;
    ez_nfc_reader_stop(reader);
    nfc_free(reader->nfc);
    free(reader);
}

/* Each phase reports exactly once; later events are ignored. */
static void ez_nfc_reader_report(EzNfcReader* reader, EzNfcReaderEvent event) {
    if(reader->reported) return;
    reader->reported = true;
    if(reader->callback) reader->callback(event, reader->context);
}

/* ---- phase 1: scan ------------------------------------------------------- */

static void ez_nfc_scanner_callback(NfcScannerEvent event, void* context) {
    EzNfcReader* reader = context;
    if(event.type != NfcScannerEventTypeDetected) return;
    bool label_chip = false;
    for(size_t i = 0; i < event.data.protocol_num; i++) {
        if(event.data.protocols[i] == NfcProtocolMfUltralight) label_chip = true;
    }
    ez_nfc_reader_report(reader, label_chip ? EzNfcReaderFound : EzNfcReaderIncompatible);
}

void ez_nfc_reader_scan(EzNfcReader* reader) {
    ez_nfc_reader_stop(reader);
    reader->reported = false;
    reader->scanner = nfc_scanner_alloc(reader->nfc);
    nfc_scanner_start(reader->scanner, ez_nfc_scanner_callback, reader);
}

/* ---- phase 2: read ------------------------------------------------------- */

static NfcCommand ez_nfc_poller_callback(NfcGenericEvent event, void* context) {
    EzNfcReader* reader = context;
    const MfUltralightPollerEvent* ev = event.event_data;
    if(event.protocol != NfcProtocolMfUltralight || !ev) return NfcCommandContinue;

    switch(ev->type) {
    case MfUltralightPollerEventTypeRequestMode:
        ev->data->poller_mode = MfUltralightPollerModeRead;
        return NfcCommandContinue;
    case MfUltralightPollerEventTypeAuthRequest:
        /* label chips are not password protected */
        ev->data->auth_context.skip_auth = true;
        return NfcCommandContinue;
    case MfUltralightPollerEventTypeReadSuccess: {
        const MfUltralightData* data = nfc_poller_get_data(reader->poller);
        size_t pages = data ? data->pages_read : 0;
        if(pages > EZ_NFC_MAX_PAGES) pages = EZ_NFC_MAX_PAGES;
        for(size_t p = 0; p < pages; p++) {
            memcpy(&reader->memory[p * 4], data->page[p].data, 4);
        }
        reader->length = pages * 4;
        ez_nfc_reader_report(reader, EzNfcReaderRead);
        return NfcCommandStop;
    }
    case MfUltralightPollerEventTypeReadFailed:
        ez_nfc_reader_report(reader, EzNfcReaderFailed);
        return NfcCommandStop;
    default:
        return NfcCommandContinue;
    }
}

void ez_nfc_reader_read(EzNfcReader* reader) {
    ez_nfc_reader_stop(reader);
    reader->reported = false;
    reader->length = 0;
    reader->poller = nfc_poller_alloc(reader->nfc, NfcProtocolMfUltralight);
    nfc_poller_start(reader->poller, ez_nfc_poller_callback, reader);
}

void ez_nfc_reader_stop(EzNfcReader* reader) {
    if(!reader) return;
    if(reader->scanner) {
        nfc_scanner_stop(reader->scanner);
        nfc_scanner_free(reader->scanner);
        reader->scanner = NULL;
    }
    if(reader->poller) {
        nfc_poller_stop(reader->poller);
        nfc_poller_free(reader->poller);
        reader->poller = NULL;
    }
}

const uint8_t* ez_nfc_reader_memory(EzNfcReader* reader, size_t* len) {
    if(len) *len = reader->length;
    return reader->memory;
}
