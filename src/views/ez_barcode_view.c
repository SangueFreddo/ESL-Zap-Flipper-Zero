/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 *
 * Barcode entry: one letter + 16 digits, edited like a combination lock.
 * The model is decoded live so the user immediately sees if it matches.
 */
#include "ez_views.h"
#include "../proto/ez_proto.h"
#include "../proto/ez_models.h"

#include <gui/elements.h>
#include <stdio.h>
#include <string.h>

#define EZ_CELL 7
#define EZ_CODE_X ((128 - EZ_BARCODE_LEN * EZ_CELL) / 2)
#define EZ_BC_FIRST 2 /* first editable position (digit 3) */
#define EZ_BC_LAST 15 /* last editable position (digit 16) */

typedef struct EzBarcodeState EzBarcodeState;

typedef struct {
    EzBarcodeState* self;
    char code[EZ_BARCODE_LEN + 1];
    uint8_t cursor;
    EzString error; /* S_COUNT = none */
} EzBarcodeModel;

struct EzBarcodeState {
    View* view;
    EzViewCallback cb;
    void* ctx;
    uint32_t done_event;
};

static void ez_barcode_draw(Canvas* canvas, void* model_ptr) {
    EzBarcodeModel* m = model_ptr;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, T(S_BC_TITLE));

    /* the code: positions 1, 2 and 17 are not needed and drawn as hatched
     * "-" cells, the cursor cell is inverted */
    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < EZ_BARCODE_LEN; i++) {
        uint8_t x = (uint8_t)(EZ_CODE_X + i * EZ_CELL);
        bool skipped = i < EZ_BC_FIRST || i > EZ_BC_LAST;
        if(skipped) {
            for(uint8_t yy = 21; yy < 31; yy++)
                for(uint8_t xx = x + 1; xx < x + EZ_CELL - 1; xx++)
                    if(((xx + yy) & 3U) == 0) canvas_draw_dot(canvas, xx, yy);
            canvas_draw_str_aligned(canvas, x + EZ_CELL / 2 + 1, 29, AlignCenter, AlignBottom, "-");
            continue;
        }
        char s[2] = {m->code[i], '\0'};
        if(i == m->cursor) {
            canvas_draw_box(canvas, x, 20, EZ_CELL, 11);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str_aligned(canvas, x + EZ_CELL / 2 + 1, 29, AlignCenter, AlignBottom, s);
            canvas_set_color(canvas, ColorBlack);
            /* up / down markers */
            uint8_t cx = (uint8_t)(x + EZ_CELL / 2);
            canvas_draw_line(canvas, cx, 15, cx, 15);
            canvas_draw_line(canvas, cx - 1, 16, cx + 1, 16);
            canvas_draw_line(canvas, cx - 2, 17, cx + 2, 17);
            canvas_draw_line(canvas, cx - 2, 33, cx + 2, 33);
            canvas_draw_line(canvas, cx - 1, 34, cx + 1, 34);
            canvas_draw_line(canvas, cx, 35, cx, 35);
        } else {
            canvas_draw_str_aligned(canvas, x + EZ_CELL / 2 + 1, 29, AlignCenter, AlignBottom, s);
        }
        /* group separator between address and model fields */
        if(i == 11) canvas_draw_dot(canvas, x + EZ_CELL, 32);
    }
    /* underline the address (3..12) and the model (13..16) */
    canvas_draw_line(canvas, EZ_CODE_X + 2 * EZ_CELL, 37, EZ_CODE_X + 12 * EZ_CELL - 2, 37);
    canvas_draw_line(canvas, EZ_CODE_X + 12 * EZ_CELL, 37, EZ_CODE_X + 16 * EZ_CELL - 2, 37);

    /* live model decode from digits 13..16 */
    char line[40];
    uint16_t code = 0;
    for(uint8_t i = 12; i < 16; i++)
        code = (uint16_t)(code * 10U + (uint16_t)(m->code[i] - '0'));
    const EzModel* model = ez_model_find(code);
    if(m->error != S_COUNT) {
        canvas_draw_rbox(canvas, 4, 40, 120, 12, 2);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 64, 49, AlignCenter, AlignBottom, T(m->error));
        canvas_set_color(canvas, ColorBlack);
    } else {
        if(!model) {
            snprintf(line, sizeof(line), "%04u: %s", code, T(S_BC_UNKNOWN));
        } else if(model->kind == EzKindSegment) {
            snprintf(line, sizeof(line), "%s (%s)", model->name, T(S_BC_SEGMENT));
        } else {
            snprintf(
                line,
                sizeof(line),
                "%s %ux%u %s",
                model->name,
                model->width,
                model->height,
                model->accent == EzAccentRed    ? "R" :
                model->accent == EzAccentYellow ? "Y" :
                                                  "BW");
        }
        canvas_draw_str_aligned(canvas, 64, 49, AlignCenter, AlignBottom, line);
    }

    canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, T(S_BC_HINT));
}

static void ez_barcode_step(EzBarcodeModel* m, int8_t dir) {
    char* c = &m->code[m->cursor];
    int v = (*c - '0' + dir + 10) % 10;
    *c = (char)('0' + v);
}

static bool ez_barcode_input(InputEvent* event, void* context) {
    EzBarcodeState* st = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    bool consumed = true;
    bool done = false;

    with_view_model(
        st->view,
        EzBarcodeModel * m,
        {
            m->error = S_COUNT;
            switch(event->key) {
            case InputKeyUp:
                ez_barcode_step(m, 1);
                break;
            case InputKeyDown:
                ez_barcode_step(m, -1);
                break;
            case InputKeyLeft:
                if(m->cursor > EZ_BC_FIRST) m->cursor--;
                break;
            case InputKeyRight:
                if(m->cursor < EZ_BC_LAST) m->cursor++;
                break;
            case InputKeyOk:
                if(event->type == InputTypeShort) done = true;
                break;
            default:
                consumed = false;
                break;
            }
        },
        true);

    if(done && st->cb) st->cb(st->done_event, st->ctx);
    return consumed;
}

View* ez_barcode_view_alloc(void) {
    View* view = view_alloc();
    EzBarcodeState* st = malloc(sizeof(EzBarcodeState));
    memset(st, 0, sizeof(EzBarcodeState));
    st->view = view;
    view_set_context(view, st);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(EzBarcodeModel));
    with_view_model(view, EzBarcodeModel * m, { m->self = st; }, false);
    view_set_draw_callback(view, ez_barcode_draw);
    view_set_input_callback(view, ez_barcode_input);
    ez_barcode_view_set(view, NULL);
    return view;
}

void ez_barcode_view_free(View* view) {
    EzBarcodeState* st = NULL;
    with_view_model(view, EzBarcodeModel * m, { st = m->self; }, false);
    view_free(view);
    free(st);
}

void ez_barcode_view_set_callback(View* view, EzViewCallback cb, void* ctx, uint32_t done_event) {
    EzBarcodeState* st = NULL;
    with_view_model(view, EzBarcodeModel * m, { st = m->self; }, false);
    st->cb = cb;
    st->ctx = ctx;
    st->done_event = done_event;
}

void ez_barcode_view_set(View* view, const char* barcode) {
    with_view_model(
        view,
        EzBarcodeModel * m,
        {
            if(barcode && ez_barcode_is_valid(barcode)) {
                memcpy(m->code, barcode, EZ_BARCODE_LEN);
                if(m->code[0] >= 'a') m->code[0] = (char)(m->code[0] - 32);
            } else {
                /* only digits 3..16 are typed, the rest stays unknown */
                memcpy(m->code, "--00000000000000-", EZ_BARCODE_LEN);
            }
            m->code[EZ_BARCODE_LEN] = '\0';
            m->cursor = EZ_BC_FIRST;
            m->error = S_COUNT;
        },
        true);
}

void ez_barcode_view_get(View* view, char* out) {
    with_view_model(
        view,
        EzBarcodeModel * m,
        {
            memcpy(out, m->code, EZ_BARCODE_LEN);
            out[EZ_BARCODE_LEN] = '\0';
        },
        false);
}

void ez_barcode_view_set_error(View* view, EzString error) {
    with_view_model(view, EzBarcodeModel * m, { m->error = error; }, true);
}
