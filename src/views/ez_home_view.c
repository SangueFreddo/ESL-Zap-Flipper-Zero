/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 *
 * Home screen: one big icon at a time, Left/Right to browse, OK to open,
 * Up/Down to switch the active tag shown in the top bar.
 */
#include "ez_views.h"

#include <gui/elements.h>
#include <string.h>
#include "esl_zap_icons.h"

typedef struct EzHomeState EzHomeState;

typedef struct {
    EzHomeState* self;
    uint8_t index;
    char name[24];
    char info[16];
    bool has_tag;
    bool can_switch;
} EzHomeModel;

struct EzHomeState {
    View* view;
    EzViewCallback cb;
    void* ctx;
    uint32_t select_base;
    uint32_t prev_event;
    uint32_t next_event;
};

static const Icon* const ez_home_icons[EzHomeCount] = {
    &I_home_image_32x32,
    &I_home_text_32x32,
    &I_home_test_32x32,
    &I_home_tags_32x32,
    &I_home_page_32x32,
    &I_home_settings_32x32,
    &I_home_help_32x32,
};

static const EzString ez_home_labels[EzHomeCount] = {
    S_HOME_IMAGE,
    S_HOME_TEXT,
    S_HOME_TEST,
    S_HOME_TAGS,
    S_HOME_PAGE,
    S_HOME_SETTINGS,
    S_HOME_HELP,
};

static void ez_draw_arrow(Canvas* canvas, uint8_t x, uint8_t y, bool left) {
    for(uint8_t i = 0; i < 4; i++) {
        uint8_t col = left ? (uint8_t)(x + i) : (uint8_t)(x + 3 - i);
        canvas_draw_line(canvas, col, y - i, col, y + i);
    }
}

static void ez_home_draw(Canvas* canvas, void* model_ptr) {
    EzHomeModel* m = model_ptr;
    canvas_clear(canvas);

    /* top bar with the active tag */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rbox(canvas, 0, 0, 128, 12, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    if(m->has_tag) {
        canvas_draw_str(canvas, m->can_switch ? 10 : 3, 9, m->name);
        canvas_draw_str_aligned(canvas, 125, 9, AlignRight, AlignBottom, m->info);
        if(m->can_switch) {
            /* small up/down hint */
            canvas_draw_line(canvas, 4, 3, 4, 8);
            canvas_draw_line(canvas, 3, 4, 5, 4);
            canvas_draw_line(canvas, 3, 7, 5, 7);
        }
    } else {
        canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignBottom, T(S_NO_TAG));
    }
    canvas_set_color(canvas, ColorBlack);

    /* carousel */
    uint8_t i = m->index % EzHomeCount;
    canvas_draw_icon(canvas, 48, 14, ez_home_icons[i]);
    ez_draw_arrow(canvas, 14, 30, true);
    ez_draw_arrow(canvas, 110, 30, false);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 57, AlignCenter, AlignBottom, T(ez_home_labels[i]));

    /* page dots */
    const uint8_t spacing = 6;
    uint8_t x = (uint8_t)(64 - (EzHomeCount * spacing) / 2 + 2);
    for(uint8_t k = 0; k < EzHomeCount; k++) {
        if(k == i) {
            canvas_draw_box(canvas, x + k * spacing - 1, 60, 3, 3);
        } else {
            canvas_draw_dot(canvas, x + k * spacing, 61);
        }
    }
}

static bool ez_home_input(InputEvent* event, void* context) {
    EzHomeState* st = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    bool consumed = true;
    uint8_t index = 0;
    switch(event->key) {
    case InputKeyLeft:
    case InputKeyRight:
        with_view_model(
            st->view,
            EzHomeModel * m,
            {
                if(event->key == InputKeyLeft)
                    m->index = (uint8_t)((m->index + EzHomeCount - 1) % EzHomeCount);
                else
                    m->index = (uint8_t)((m->index + 1) % EzHomeCount);
            },
            true);
        break;
    case InputKeyOk:
        if(event->type != InputTypeShort) break;
        with_view_model(st->view, EzHomeModel * m, { index = m->index; }, false);
        if(st->cb) st->cb(st->select_base + index, st->ctx);
        break;
    case InputKeyUp:
    case InputKeyDown:
        if(event->type != InputTypeShort) break;
        if(st->cb) st->cb(event->key == InputKeyUp ? st->prev_event : st->next_event, st->ctx);
        break;
    default:
        consumed = false;
        break;
    }
    return consumed;
}

View* ez_home_view_alloc(void) {
    View* view = view_alloc();
    EzHomeState* st = malloc(sizeof(EzHomeState));
    memset(st, 0, sizeof(EzHomeState));
    st->view = view;
    view_set_context(view, st);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(EzHomeModel));
    with_view_model(view, EzHomeModel * m, { m->self = st; }, false);
    view_set_draw_callback(view, ez_home_draw);
    view_set_input_callback(view, ez_home_input);
    return view;
}

void ez_home_view_free(View* view) {
    EzHomeState* st = NULL;
    with_view_model(view, EzHomeModel * m, { st = m->self; }, false);
    view_free(view);
    free(st);
}

void ez_home_view_set_callback(
    View* view,
    EzViewCallback cb,
    void* ctx,
    uint32_t select_base,
    uint32_t prev_event,
    uint32_t next_event) {
    EzHomeState* st = NULL;
    with_view_model(view, EzHomeModel * m, { st = m->self; }, false);
    st->cb = cb;
    st->ctx = ctx;
    st->select_base = select_base;
    st->prev_event = prev_event;
    st->next_event = next_event;
}

void ez_home_view_set(
    View* view,
    uint8_t index,
    const char* tag_name,
    const char* tag_info,
    bool can_switch) {
    with_view_model(
        view,
        EzHomeModel * m,
        {
            m->index = (uint8_t)(index % EzHomeCount);
            m->has_tag = tag_name != NULL;
            m->can_switch = can_switch;
            strncpy(m->name, tag_name ? tag_name : "", sizeof(m->name) - 1);
            m->name[sizeof(m->name) - 1] = '\0';
            strncpy(m->info, tag_info ? tag_info : "", sizeof(m->info) - 1);
            m->info[sizeof(m->info) - 1] = '\0';
        },
        true);
}
