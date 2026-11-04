/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 *
 * Picking an image, typing text, preview and render options.
 */
#include "../esl_zap.h"

#include <stdio.h>
#include <string.h>

#define EZ_PICK_BROWSE 0
#define EZ_PICK_LEAVE 1

/* ---- image picker -------------------------------------------------------- */

static uint16_t ez_count_images(Storage* storage) {
    uint16_t count = 0;
    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, EZ_IMAGES_DIR)) {
        FileInfo info;
        char name[64];
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(file_info_is_dir(&info)) continue;
            size_t len = strlen(name);
            if(len > 4 && (strcmp(name + len - 4, ".bmp") == 0 ||
                           strcmp(name + len - 4, ".BMP") == 0)) {
                count++;
            }
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    return count;
}

void ez_scene_pick_image_on_enter(void* context) {
    EslZap* app = context;
    if(scene_manager_get_scene_state(app->scene_manager, EzScenePickImage) == EZ_PICK_LEAVE) {
        scene_manager_set_scene_state(app->scene_manager, EzScenePickImage, EZ_PICK_BROWSE);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    if(ez_count_images(app->storage) == 0) {
        scene_manager_set_scene_state(app->scene_manager, EzScenePickImage, EZ_PICK_LEAVE);
        ez_show_notice(app, S_NO_IMAGES, S_NO_IMAGES_HINT, EzNoticeBack);
        return;
    }

    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(&options, ".bmp", NULL);
    options.base_path = EZ_IMAGES_DIR;
    options.hide_dot_files = true;
    options.skip_assets = true;

    if(dialog_file_browser_show(app->dialogs, app->image_path, app->image_path, &options)) {
        app->spec.kind = EzSourceBmp;
        strncpy(app->spec.path, furi_string_get_cstr(app->image_path), sizeof(app->spec.path) - 1);
        app->spec.path[sizeof(app->spec.path) - 1] = '\0';
        scene_manager_next_scene(app->scene_manager, EzScenePreview);
    } else {
        scene_manager_previous_scene(app->scene_manager);
    }
}

bool ez_scene_pick_image_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void ez_scene_pick_image_on_exit(void* context) {
    UNUSED(context);
}

/* ---- text ---------------------------------------------------------------- */

static void ez_text_input_done(void* context) {
    EslZap* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, EzEventTextDone);
}

void ez_scene_text_on_enter(void* context) {
    EslZap* app = context;
    if(!app->text_buf[0]) strncpy(app->text_buf, "Hello!", EZ_TEXT_MAX);
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, T(S_TEXT_TITLE));
    text_input_set_result_callback(
        app->text_input, ez_text_input_done, app, app->text_buf, EZ_TEXT_MAX + 1, false);
    view_dispatcher_switch_to_view(app->view_dispatcher, EzViewTextInput);
}

bool ez_scene_text_on_event(void* context, SceneManagerEvent event) {
    EslZap* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == EzEventTextDone) {
        app->spec.kind = EzSourceText;
        scene_manager_next_scene(app->scene_manager, EzScenePreview);
        return true;
    }
    return false;
}

void ez_scene_text_on_exit(void* context) {
    EslZap* app = context;
    text_input_reset(app->text_input);
}

/* ---- preview ------------------------------------------------------------- */

/* Copies the current text into the render spec. */
static void ez_prepare_spec(EslZap* app, const EzModel* model) {
    if(app->spec.kind == EzSourceText) {
        strncpy(app->spec.text, app->text_buf, EZ_TEXT_MAX);
        app->spec.text[EZ_TEXT_MAX] = '\0';
    } else if(app->spec.kind == EzSourceTestCard) {
        snprintf(app->spec.text, sizeof(app->spec.text), "ESL Zap|%ux%u", model->width, model->height);
    }
}

static void ez_render_preview(EslZap* app) {
    EzModel model;
    EzTag* tag = ez_active_tag(app);
    if(!tag || !ez_tag_model(tag, &model)) {
        ez_preview_view_set(app->preview_view, NULL, "", T(S_NEED_SIZE));
        return;
    }
    ez_prepare_spec(app, &model);

    /* fit the panel into the preview box, keeping its aspect ratio */
    uint32_t pw = EZ_PREVIEW_MAX_W;
    uint32_t ph = ((uint32_t)model.height * EZ_PREVIEW_MAX_W) / model.width;
    if(ph > EZ_PREVIEW_MAX_H) {
        ph = EZ_PREVIEW_MAX_H;
        pw = ((uint32_t)model.width * EZ_PREVIEW_MAX_H) / model.height;
    }
    if(pw < 1) pw = 1;
    if(ph < 1) ph = 1;

    char info[20];
    snprintf(info, sizeof(info), "%ux%u p%u", model.width, model.height, app->page);

    const char* error = NULL;
    EzSource* src = ez_source_create(&app->spec, (uint16_t)pw, (uint16_t)ph, app->storage, &error);
    if(!src) {
        ez_preview_view_set(app->preview_view, NULL, info, error ? error : T(S_ERR_IMAGE));
        return;
    }

    EzPreviewImage* image = malloc(sizeof(EzPreviewImage));
    uint8_t* row = malloc(pw);
    memset(image, 0, sizeof(EzPreviewImage));
    image->w = (uint8_t)pw;
    image->h = (uint8_t)ph;
    bool ok = true;
    for(uint32_t y = 0; y < ph && ok; y++) {
        ok = ez_source_row(src, row);
        for(uint32_t x = 0; ok && x < pw; x++) {
            bool ink = row[x] == EZ_PX_BLACK || (row[x] == EZ_PX_ACCENT && ((x + y) & 1U));
            if(ink) {
                uint32_t i = y * pw + x;
                image->bits[i >> 3] |= (uint8_t)(1U << (i & 7U));
            }
        }
    }
    ez_source_free(src);
    ez_preview_view_set(app->preview_view, ok ? image : NULL, info, T(S_ERR_IMAGE));
    free(row);
    free(image);
}

void ez_scene_preview_on_enter(void* context) {
    EslZap* app = context;
    ez_render_preview(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, EzViewPreview);
}

bool ez_scene_preview_on_event(void* context, SceneManagerEvent event) {
    EslZap* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event == EzEventPreviewSend) {
        app->page_job = false;
        scene_manager_next_scene(app->scene_manager, EzSceneSend);
        return true;
    }
    if(event.event == EzEventPreviewOptions) {
        scene_manager_next_scene(app->scene_manager, EzSceneOptions);
        return true;
    }
    return false;
}

void ez_scene_preview_on_exit(void* context) {
    UNUSED(context);
}

/* ---- options ------------------------------------------------------------- */

typedef enum {
    EzOptFit,
    EzOptDither,
    EzOptInvert,
    EzOptAccent,
    EzOptTextColor,
    EzOptFrame,
    EzOptPage,
} EzOpt;

static const EzString ez_fit_names[EzFitCount] = {S_FIT_CONTAIN, S_FIT_COVER, S_FIT_STRETCH};

static EzAccent ez_active_accent(EslZap* app) {
    EzTag* tag = ez_active_tag(app);
    EzModel model;
    if(!tag || !ez_tag_model(tag, &model)) return EzAccentNone;
    return model.accent;
}

static const char* ez_on_off(bool on) {
    return on ? T(S_ON) : T(S_OFF);
}

static void ez_option_changed(VariableItem* item) {
    EslZap* app = variable_item_get_context(item);
    uint8_t idx = variable_item_list_get_selected_item_index(app->list);
    if(idx >= app->opt_count) return;
    uint8_t v = variable_item_get_current_value_index(item);
    EzAccent accent = ez_active_accent(app);
    char text[8];

    switch(app->opt_types[idx]) {
    case EzOptFit:
        app->spec.fit = (EzFit)v;
        variable_item_set_current_value_text(item, T(ez_fit_names[v]));
        break;
    case EzOptDither:
        app->spec.dither = v;
        variable_item_set_current_value_text(item, ez_on_off(v));
        break;
    case EzOptInvert:
        app->spec.invert = v;
        variable_item_set_current_value_text(item, ez_on_off(v));
        break;
    case EzOptFrame:
        app->spec.frame = v;
        variable_item_set_current_value_text(item, ez_on_off(v));
        break;
    case EzOptAccent:
        app->spec.accent = v ? accent : EzAccentNone;
        variable_item_set_current_value_text(item, ez_on_off(v));
        break;
    case EzOptTextColor:
        app->spec.accent_text = v;
        app->spec.accent = accent;
        variable_item_set_current_value_text(
            item, v ? T(accent == EzAccentYellow ? S_YELLOW : S_RED) : T(S_BLACK));
        break;
    case EzOptPage:
        app->page = v;
        snprintf(text, sizeof(text), "%u", v);
        variable_item_set_current_value_text(item, text);
        break;
    }
}

static void ez_add_option(EslZap* app, EzOpt type) {
    if(app->opt_count >= sizeof(app->opt_types)) return;
    EzAccent accent = ez_active_accent(app);
    VariableItem* item = NULL;
    uint8_t value = 0;
    char text[8];
    const char* value_text = "";

    switch(type) {
    case EzOptFit:
        item = variable_item_list_add(app->list, T(S_OPT_FIT), EzFitCount, ez_option_changed, app);
        value = app->spec.fit;
        value_text = T(ez_fit_names[value]);
        break;
    case EzOptDither:
        item = variable_item_list_add(app->list, T(S_OPT_DITHER), 2, ez_option_changed, app);
        value = app->spec.dither;
        value_text = ez_on_off(value);
        break;
    case EzOptInvert:
        item = variable_item_list_add(app->list, T(S_OPT_INVERT), 2, ez_option_changed, app);
        value = app->spec.invert;
        value_text = ez_on_off(value);
        break;
    case EzOptFrame:
        item = variable_item_list_add(app->list, T(S_OPT_FRAME), 2, ez_option_changed, app);
        value = app->spec.frame;
        value_text = ez_on_off(value);
        break;
    case EzOptAccent:
        item = variable_item_list_add(app->list, T(S_OPT_ACCENT), 2, ez_option_changed, app);
        value = app->spec.accent != EzAccentNone;
        value_text = ez_on_off(value);
        break;
    case EzOptTextColor:
        item = variable_item_list_add(app->list, T(S_OPT_TEXTCOLOR), 2, ez_option_changed, app);
        value = app->spec.accent_text;
        value_text = value ? T(accent == EzAccentYellow ? S_YELLOW : S_RED) : T(S_BLACK);
        break;
    case EzOptPage:
        item = variable_item_list_add(app->list, T(S_OPT_PAGE), 8, ez_option_changed, app);
        value = app->page & 7U;
        snprintf(text, sizeof(text), "%u", value);
        value_text = text;
        break;
    }
    variable_item_set_current_value_index(item, value);
    variable_item_set_current_value_text(item, value_text);
    app->opt_types[app->opt_count++] = (uint8_t)type;
}

void ez_scene_options_on_enter(void* context) {
    EslZap* app = context;
    bool color = ez_active_accent(app) != EzAccentNone;
    variable_item_list_reset(app->list);
    app->opt_count = 0;

    switch(app->spec.kind) {
    case EzSourceBmp:
        ez_add_option(app, EzOptFit);
        ez_add_option(app, EzOptDither);
        ez_add_option(app, EzOptInvert);
        if(color) ez_add_option(app, EzOptAccent);
        break;
    case EzSourceText:
        ez_add_option(app, EzOptFrame);
        ez_add_option(app, EzOptInvert);
        if(color) ez_add_option(app, EzOptTextColor);
        break;
    default:
        if(color) ez_add_option(app, EzOptAccent);
        break;
    }
    ez_add_option(app, EzOptPage);
    view_dispatcher_switch_to_view(app->view_dispatcher, EzViewList);
}

bool ez_scene_options_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void ez_scene_options_on_exit(void* context) {
    EslZap* app = context;
    variable_item_list_reset(app->list);
}
