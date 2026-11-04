/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 *
 * Shows what the label will display before anything is transmitted.
 */
#include "ez_views.h"

#include <gui/elements.h>
#include <string.h>

typedef struct EzPreviewState EzPreviewState;

typedef struct {
    EzPreviewState* self;
    EzPreviewImage image;
    bool has_image;
    char info[20];
    char error[48];
} EzPreviewModel;

struct EzPreviewState {
    View* view;
    EzViewCallback cb;
    void* ctx;
    uint32_t send_event;
    uint32_t options_event;
};

static void ez_preview_draw(Canvas* canvas, void* model_ptr) {
    EzPreviewModel* m = model_ptr;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(m->has_image && m->image.w && m->image.h) {
        const uint8_t w = m->image.w, h = m->image.h;
        const uint8_t x0 = (uint8_t)((128 - w) / 2);
        const uint8_t y0 = (uint8_t)(1 + (EZ_PREVIEW_MAX_H - h) / 2);
        canvas_draw_frame(canvas, x0 - 1, y0 - 1, w + 2, h + 2);
        for(uint8_t y = 0; y < h; y++) {
            for(uint8_t x = 0; x < w; x++) {
                uint32_t i = (uint32_t)y * w + x;
                if(m->image.bits[i >> 3] & (1U << (i & 7U))) canvas_draw_dot(canvas, x0 + x, y0 + y);
            }
        }
    } else {
        canvas_set_font(canvas, FontSecondary);
        elements_multiline_text_aligned(canvas, 64, 20, AlignCenter, AlignCenter, m->error);
    }

    canvas_set_font(canvas, FontSecondary);
    elements_button_left(canvas, T(S_OPTIONS));
    if(m->has_image) elements_button_center(canvas, T(S_SEND));
    canvas_draw_str_aligned(canvas, 127, 63, AlignRight, AlignBottom, m->info);
}

static bool ez_preview_input(InputEvent* event, void* context) {
    EzPreviewState* st = context;
    if(event->type != InputTypeShort) return false;
    if(event->key == InputKeyOk) {
        bool ready = false;
        with_view_model(st->view, EzPreviewModel * m, { ready = m->has_image; }, false);
        if(ready && st->cb) st->cb(st->send_event, st->ctx);
        return true;
    }
    if(event->key == InputKeyLeft) {
        if(st->cb) st->cb(st->options_event, st->ctx);
        return true;
    }
    return false;
}

View* ez_preview_view_alloc(void) {
    View* view = view_alloc();
    EzPreviewState* st = malloc(sizeof(EzPreviewState));
    memset(st, 0, sizeof(EzPreviewState));
    st->view = view;
    view_set_context(view, st);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(EzPreviewModel));
    with_view_model(view, EzPreviewModel * m, { m->self = st; }, false);
    view_set_draw_callback(view, ez_preview_draw);
    view_set_input_callback(view, ez_preview_input);
    return view;
}

void ez_preview_view_free(View* view) {
    EzPreviewState* st = NULL;
    with_view_model(view, EzPreviewModel * m, { st = m->self; }, false);
    view_free(view);
    free(st);
}

void ez_preview_view_set_callback(
    View* view,
    EzViewCallback cb,
    void* ctx,
    uint32_t send_event,
    uint32_t options_event) {
    EzPreviewState* st = NULL;
    with_view_model(view, EzPreviewModel * m, { st = m->self; }, false);
    st->cb = cb;
    st->ctx = ctx;
    st->send_event = send_event;
    st->options_event = options_event;
}

void ez_preview_view_set(
    View* view,
    const EzPreviewImage* image,
    const char* info,
    const char* error) {
    with_view_model(
        view,
        EzPreviewModel * m,
        {
            m->has_image = image != NULL;
            if(image) memcpy(&m->image, image, sizeof(EzPreviewImage));
            strncpy(m->info, info ? info : "", sizeof(m->info) - 1);
            m->info[sizeof(m->info) - 1] = '\0';
            strncpy(m->error, error ? error : "", sizeof(m->error) - 1);
            m->error[sizeof(m->error) - 1] = '\0';
        },
        true);
}
