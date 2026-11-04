/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 *
 * Transmission screen: animated beam, phase, progress bar and result.
 */
#include "ez_views.h"

#include <furi.h>
#include <gui/elements.h>
#include <stdio.h>
#include <string.h>
#include "esl_zap_icons.h"

typedef struct EzSendState EzSendState;

typedef struct {
    EzSendState* self;
    EzProgress progress;
    char target[24];
} EzSendModel;

struct EzSendState {
    View* view;
    EzViewCallback cb;
    void* ctx;
    uint32_t again_event;
    uint32_t home_event;
};

static bool ez_send_finished(const EzProgress* p) {
    return p->phase == EzPhaseDone || p->phase == EzPhaseError || p->phase == EzPhaseCancelled;
}

static const char* ez_error_text(const EzProgress* p) {
    switch(p->error) {
    case EzErrorIrBusy:
        return T(S_ERR_IR);
    case EzErrorTooBig:
        return T(S_ERR_BIG);
    case EzErrorMemory:
        return T(S_ERR_MEM);
    case EzErrorImage:
        return p->detail ? p->detail : T(S_ERR_IMAGE);
    default:
        return "";
    }
}

static void ez_send_draw(Canvas* canvas, void* model_ptr) {
    EzSendModel* m = model_ptr;
    const EzProgress* p = &m->progress;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(ez_send_finished(p)) {
        bool ok = p->phase == EzPhaseDone;
        canvas_draw_icon(canvas, 56, 2, ok ? &I_tx_ok_15x15 : &I_tx_fail_15x15);
        canvas_set_font(canvas, FontPrimary);
        const char* title = ok                             ? T(S_DONE) :
                            (p->phase == EzPhaseCancelled) ? T(S_STOPPED) :
                                                             T(S_FAILED);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignBottom, title);
        canvas_set_font(canvas, FontSecondary);
        const char* sub = ok ? T(S_DONE_SUB) : (p->phase == EzPhaseError ? ez_error_text(p) : "");
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignBottom, sub);
        elements_button_left(canvas, T(S_AGAIN));
        elements_button_right(canvas, T(S_HOME));
        return;
    }

    /* header: who we are talking to */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, m->target);

    /* Flipper -> beam -> tag */
    canvas_draw_icon(canvas, 4, 18, &I_tx_flipper_20x14);
    canvas_draw_icon(canvas, 100, 16, &I_tx_tag_24x18);
    if(p->phase != EzPhasePrepare) {
        uint32_t frame = (furi_get_tick() / 90U) % 4U;
        for(uint8_t k = 0; k < 9; k++) {
            uint8_t x = (uint8_t)(30 + k * 7);
            if(((k + 4U - frame) % 4U) == 0) {
                canvas_draw_line(canvas, x, 21, x + 2, 24);
                canvas_draw_line(canvas, x + 2, 25, x, 28);
            } else {
                canvas_draw_dot(canvas, x + 1, 25);
            }
        }
    }

    /* phase */
    char line[40];
    canvas_set_font(canvas, FontSecondary);
    switch(p->phase) {
    case EzPhasePrepare:
        snprintf(line, sizeof(line), "%s", T(S_PH_PREP));
        break;
    case EzPhaseWake:
        snprintf(line, sizeof(line), "%s", T(S_PH_WAKE));
        break;
    case EzPhaseData:
        if(p->strips > 1)
            snprintf(line, sizeof(line), "%s %u/%u", T(S_PH_PART), p->strip, p->strips);
        else
            snprintf(line, sizeof(line), "%s", T(S_PH_DATA));
        break;
    default:
        snprintf(line, sizeof(line), "%s", T(S_PH_FINISH));
        break;
    }
    canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignBottom, line);

    elements_progress_bar(canvas, 4, 45, 120, (float)p->percent / 100.0f);
    canvas_draw_str_aligned(canvas, 64, 61, AlignCenter, AlignBottom, T(S_TX_AIM));
}

static bool ez_send_input(InputEvent* event, void* context) {
    EzSendState* st = context;
    if(event->type != InputTypeShort) return false;
    bool finished = false;
    with_view_model(
        st->view, EzSendModel * m, { finished = ez_send_finished(&m->progress); }, false);
    if(!finished) return false; /* Back is handled by the scene (cancel) */
    if(event->key == InputKeyLeft) {
        if(st->cb) st->cb(st->again_event, st->ctx);
        return true;
    }
    if(event->key == InputKeyRight || event->key == InputKeyOk) {
        if(st->cb) st->cb(st->home_event, st->ctx);
        return true;
    }
    return false;
}

View* ez_send_view_alloc(void) {
    View* view = view_alloc();
    EzSendState* st = malloc(sizeof(EzSendState));
    memset(st, 0, sizeof(EzSendState));
    st->view = view;
    view_set_context(view, st);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(EzSendModel));
    with_view_model(view, EzSendModel * m, { m->self = st; }, false);
    view_set_draw_callback(view, ez_send_draw);
    view_set_input_callback(view, ez_send_input);
    return view;
}

void ez_send_view_free(View* view) {
    EzSendState* st = NULL;
    with_view_model(view, EzSendModel * m, { st = m->self; }, false);
    view_free(view);
    free(st);
}

void ez_send_view_set_callback(
    View* view,
    EzViewCallback cb,
    void* ctx,
    uint32_t again_event,
    uint32_t home_event) {
    EzSendState* st = NULL;
    with_view_model(view, EzSendModel * m, { st = m->self; }, false);
    st->cb = cb;
    st->ctx = ctx;
    st->again_event = again_event;
    st->home_event = home_event;
}

void ez_send_view_reset(View* view, const char* target) {
    with_view_model(
        view,
        EzSendModel * m,
        {
            memset(&m->progress, 0, sizeof(m->progress));
            m->progress.phase = EzPhasePrepare;
            strncpy(m->target, target ? target : "", sizeof(m->target) - 1);
            m->target[sizeof(m->target) - 1] = '\0';
        },
        true);
}

void ez_send_view_update(View* view, const EzProgress* progress) {
    with_view_model(view, EzSendModel * m, { m->progress = *progress; }, true);
}
