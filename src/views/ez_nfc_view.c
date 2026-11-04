/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 *
 * NFC search screen: animation and instructions while searching, then the
 * label found (Save) or the reason it cannot be used (Retry). Cancel is
 * always available on the left button.
 */
#include "ez_views.h"

#include <furi.h>
#include <gui/elements.h>
#include <string.h>
#include "esl_zap_icons.h"

typedef struct EzNfcViewCtx EzNfcViewCtx;

typedef struct {
    EzNfcViewCtx* self;
    EzNfcViewState state;
    uint8_t tick;
    char line1[32];
    char line2[24];
    EzString title;
    EzString reason;
} EzNfcModel;

struct EzNfcViewCtx {
    View* view;
    EzViewCallback cb;
    void* ctx;
    uint32_t cancel_event;
    uint32_t save_event;
    uint32_t retry_event;
};

/* Concentric NFC waves between the Flipper and the label. */
static void ez_nfc_waves(Canvas* canvas, uint8_t cx, uint8_t cy, uint8_t phase) {
    for(uint8_t r = 0; r < 3; r++) {
        if(((r + phase) % 3) == 2) continue; /* one wave blinks out: motion */
        uint8_t rad = (uint8_t)(5 + r * 5);
        for(int8_t dy = -(int8_t)rad + 2; dy <= (int8_t)rad - 2; dy++) {
            /* right half of a circle, drawn as dots */
            int32_t dx2 = (int32_t)rad * rad - (int32_t)dy * dy;
            uint8_t dx = 0;
            while((int32_t)(dx + 1) * (dx + 1) <= dx2)
                dx++;
            canvas_draw_dot(canvas, cx + dx, cy + dy);
        }
    }
}

static void ez_nfc_draw(Canvas* canvas, void* model_ptr) {
    EzNfcModel* m = model_ptr;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(m->state == EzNfcViewSearching) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, T(S_NFC_SCAN_TITLE));
        canvas_draw_icon(canvas, 6, 18, &I_tx_flipper_20x14);
        ez_nfc_waves(canvas, 30, 25, m->tick % 3);
        /* label slides closer and back */
        uint8_t offs = (uint8_t)((m->tick / 2) % 6);
        canvas_draw_icon(canvas, (uint8_t)(66 - offs), 16, &I_tx_tag_24x18);
        canvas_draw_str(canvas, 92 - offs, 28, "NFC");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignBottom, T(S_NFC_SCAN_HINT1));
        canvas_draw_str_aligned(canvas, 64, 53, AlignCenter, AlignBottom, T(S_NFC_SCAN_HINT2));
        elements_button_left(canvas, T(S_CANCEL));
        return;
    }

    bool ok = m->state == EzNfcViewFound;
    canvas_draw_icon(canvas, 2, 2, ok ? &I_tx_ok_15x15 : &I_tx_fail_15x15);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 22, 13, ok ? T(S_NFC_FOUND) : T(m->title));
    canvas_set_font(canvas, FontSecondary);
    if(ok) {
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignBottom, m->line1);
        canvas_draw_str_aligned(canvas, 64, 43, AlignCenter, AlignBottom, m->line2);
        elements_button_left(canvas, T(S_CANCEL));
        elements_button_right(canvas, T(S_SAVE));
    } else {
        elements_multiline_text_aligned(canvas, 64, 21, AlignCenter, AlignTop, T(m->reason));
        elements_button_left(canvas, T(S_CANCEL));
        elements_button_right(canvas, T(S_RETRY));
    }
}

static bool ez_nfc_input(InputEvent* event, void* context) {
    EzNfcViewCtx* st = context;
    if(event->type != InputTypeShort) return false;
    EzNfcViewState state = EzNfcViewSearching;
    with_view_model(st->view, EzNfcModel * m, { state = m->state; }, false);

    uint32_t ev = 0;
    if(event->key == InputKeyLeft) ev = st->cancel_event;
    if(event->key == InputKeyRight || event->key == InputKeyOk) {
        if(state == EzNfcViewFound) ev = st->save_event;
        if(state == EzNfcViewProblem) ev = st->retry_event;
    }
    if(!ev) return false; /* Back goes through the scene manager (cancel) */
    if(st->cb) st->cb(ev, st->ctx);
    return true;
}

View* ez_nfc_view_alloc(void) {
    View* view = view_alloc();
    EzNfcViewCtx* st = malloc(sizeof(EzNfcViewCtx));
    memset(st, 0, sizeof(EzNfcViewCtx));
    st->view = view;
    view_set_context(view, st);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(EzNfcModel));
    with_view_model(view, EzNfcModel * m, { m->self = st; }, false);
    view_set_draw_callback(view, ez_nfc_draw);
    view_set_input_callback(view, ez_nfc_input);
    return view;
}

void ez_nfc_view_free(View* view) {
    EzNfcViewCtx* st = NULL;
    with_view_model(view, EzNfcModel * m, { st = m->self; }, false);
    view_free(view);
    free(st);
}

void ez_nfc_view_set_callback(
    View* view,
    EzViewCallback cb,
    void* ctx,
    uint32_t cancel_event,
    uint32_t save_event,
    uint32_t retry_event) {
    EzNfcViewCtx* st = NULL;
    with_view_model(view, EzNfcModel * m, { st = m->self; }, false);
    st->cb = cb;
    st->ctx = ctx;
    st->cancel_event = cancel_event;
    st->save_event = save_event;
    st->retry_event = retry_event;
}

void ez_nfc_view_searching(View* view) {
    with_view_model(
        view,
        EzNfcModel * m,
        {
            m->state = EzNfcViewSearching;
            m->tick = 0;
        },
        true);
}

void ez_nfc_view_found(View* view, const char* line1, const char* line2) {
    with_view_model(
        view,
        EzNfcModel * m,
        {
            m->state = EzNfcViewFound;
            strncpy(m->line1, line1 ? line1 : "", sizeof(m->line1) - 1);
            m->line1[sizeof(m->line1) - 1] = '\0';
            strncpy(m->line2, line2 ? line2 : "", sizeof(m->line2) - 1);
            m->line2[sizeof(m->line2) - 1] = '\0';
        },
        true);
}

void ez_nfc_view_problem(View* view, EzString title, EzString reason) {
    with_view_model(
        view,
        EzNfcModel * m,
        {
            m->state = EzNfcViewProblem;
            m->title = title;
            m->reason = reason;
        },
        true);
}

void ez_nfc_view_tick(View* view) {
    with_view_model(
        view,
        EzNfcModel * m,
        {
            if(m->state == EzNfcViewSearching) m->tick++;
        },
        m->state == EzNfcViewSearching);
}
