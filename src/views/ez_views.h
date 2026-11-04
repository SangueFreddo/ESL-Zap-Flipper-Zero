/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 *
 * Custom views: home carousel, barcode entry, preview and transmission.
 */
#pragma once

#include <gui/view.h>
#include "../app/ez_job.h"
#include "../app/ez_i18n.h"

/* Every view reports user actions as custom event ids. */
typedef void (*EzViewCallback)(uint32_t event, void* context);

/* ---- home ---------------------------------------------------------------- */

typedef enum {
    EzHomeImage = 0,
    EzHomeText,
    EzHomeTest,
    EzHomeTags,
    EzHomePage,
    EzHomeSettings,
    EzHomeHelp,
    EzHomeCount,
} EzHomeItem;

View* ez_home_view_alloc(void);
void ez_home_view_free(View* view);
void ez_home_view_set_callback(View* view, EzViewCallback cb, void* ctx, uint32_t select_base,
                               uint32_t prev_event, uint32_t next_event);
void ez_home_view_set(View* view, uint8_t index, const char* tag_name, const char* tag_info,
                      bool can_switch);

/* ---- barcode entry ------------------------------------------------------- */

View* ez_barcode_view_alloc(void);
void ez_barcode_view_free(View* view);
void ez_barcode_view_set_callback(View* view, EzViewCallback cb, void* ctx, uint32_t done_event);
void ez_barcode_view_set(View* view, const char* barcode);
void ez_barcode_view_get(View* view, char* out);
void ez_barcode_view_set_error(View* view, EzString error);

/* ---- preview ------------------------------------------------------------- */

#define EZ_PREVIEW_MAX_W 124
#define EZ_PREVIEW_MAX_H 46
#define EZ_PREVIEW_BYTES ((EZ_PREVIEW_MAX_W * EZ_PREVIEW_MAX_H + 7) / 8)

typedef struct {
    uint8_t bits[EZ_PREVIEW_BYTES]; /* 1 = ink on the Flipper screen */
    uint8_t w;
    uint8_t h;
} EzPreviewImage;

View* ez_preview_view_alloc(void);
void ez_preview_view_free(View* view);
void ez_preview_view_set_callback(
    View* view,
    EzViewCallback cb,
    void* ctx,
    uint32_t send_event,
    uint32_t options_event);
/* image may be NULL to show `error` instead */
void ez_preview_view_set(View* view, const EzPreviewImage* image, const char* info,
                         const char* error);

/* ---- transmission -------------------------------------------------------- */

View* ez_send_view_alloc(void);
void ez_send_view_free(View* view);
void ez_send_view_set_callback(
    View* view,
    EzViewCallback cb,
    void* ctx,
    uint32_t again_event,
    uint32_t home_event);
void ez_send_view_reset(View* view, const char* target);
void ez_send_view_update(View* view, const EzProgress* progress);

/* ---- NFC search ---------------------------------------------------------- */

typedef enum {
    EzNfcViewSearching = 0,
    EzNfcViewFound,
    EzNfcViewProblem,
} EzNfcViewState;

View* ez_nfc_view_alloc(void);
void ez_nfc_view_free(View* view);
void ez_nfc_view_set_callback(
    View* view,
    EzViewCallback cb,
    void* ctx,
    uint32_t cancel_event,
    uint32_t save_event,
    uint32_t retry_event);
void ez_nfc_view_searching(View* view);
/* line1: model and size, line2: barcode */
void ez_nfc_view_found(View* view, const char* line1, const char* line2);
void ez_nfc_view_problem(View* view, EzString title, EzString reason);
/* Advances the search animation. */
void ez_nfc_view_tick(View* view);
