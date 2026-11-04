/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 *
 * Tag management: list, add (barcode + name), details, size, delete.
 */
#include "../esl_zap.h"
#include "../proto/ez_nfc.h"
#include "../app/ez_nfc_reader.h"

#include <stdio.h>
#include <string.h>

#define EZ_TAGS_ADD_ITEM 200
#define EZ_TAGS_NFC_ITEM 201
enum {
    EzTagMenuUse,
    EzTagMenuInfo,
    EzTagMenuRename,
    EzTagMenuSize,
    EzTagMenuDelete,
};

static EzTag* ez_edit_tag(EslZap* app) {
    if(app->edit_tag >= app->store.tag_count) return NULL;
    return &app->store.tags[app->edit_tag];
}

/* ---- tag list ------------------------------------------------------------ */

void ez_scene_tags_on_enter(void* context) {
    EslZap* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, T(S_TAGS_TITLE));
    char label[EZ_NAME_MAX + 4];
    for(uint8_t i = 0; i < app->store.tag_count; i++) {
        snprintf(
            label,
            sizeof(label),
            "%s%s",
            (i == app->store.settings.active) ? "> " : "",
            app->store.tags[i].name);
        submenu_add_item(app->submenu, label, i, ez_submenu_callback, app);
    }
    if(app->store.tag_count < EZ_MAX_TAGS) {
        submenu_add_item(app->submenu, T(S_TAG_ADD), EZ_TAGS_ADD_ITEM, ez_submenu_callback, app);
        submenu_add_item(
            app->submenu, T(S_TAG_ADD_NFC), EZ_TAGS_NFC_ITEM, ez_submenu_callback, app);
    }
    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, EzSceneTags));
    view_dispatcher_switch_to_view(app->view_dispatcher, EzViewSubmenu);
}

bool ez_scene_tags_on_event(void* context, SceneManagerEvent event) {
    EslZap* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event < EzEventSubmenu) return false;
    uint32_t index = event.event - EzEventSubmenu;
    scene_manager_set_scene_state(app->scene_manager, EzSceneTags, index);
    if(index == EZ_TAGS_NFC_ITEM) {
        scene_manager_next_scene(app->scene_manager, EzSceneNfcImport);
    } else if(index == EZ_TAGS_ADD_ITEM) {
        app->edit_tag = EZ_NEW_TAG;
        ez_barcode_view_set(app->barcode_view, NULL);
        scene_manager_next_scene(app->scene_manager, EzSceneBarcode);
    } else {
        app->edit_tag = (uint8_t)index;
        scene_manager_set_scene_state(app->scene_manager, EzSceneTagMenu, 0);
        scene_manager_next_scene(app->scene_manager, EzSceneTagMenu);
    }
    return true;
}

void ez_scene_tags_on_exit(void* context) {
    EslZap* app = context;
    submenu_reset(app->submenu);
}

/* ---- tag menu ------------------------------------------------------------ */

void ez_scene_tag_menu_on_enter(void* context) {
    EslZap* app = context;
    EzTag* tag = ez_edit_tag(app);
    if(!tag) {
        scene_manager_previous_scene(app->scene_manager);
        return;
    }
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, tag->name);
    submenu_add_item(app->submenu, T(S_TAG_USE), EzTagMenuUse, ez_submenu_callback, app);
    submenu_add_item(app->submenu, T(S_TAG_INFO), EzTagMenuInfo, ez_submenu_callback, app);
    submenu_add_item(app->submenu, T(S_TAG_RENAME), EzTagMenuRename, ez_submenu_callback, app);
    if(!ez_model_find(ez_barcode_model(tag->barcode))) {
        submenu_add_item(app->submenu, T(S_TAG_SIZE), EzTagMenuSize, ez_submenu_callback, app);
    }
    submenu_add_item(app->submenu, T(S_TAG_DELETE), EzTagMenuDelete, ez_submenu_callback, app);
    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, EzSceneTagMenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, EzViewSubmenu);
}

bool ez_scene_tag_menu_on_event(void* context, SceneManagerEvent event) {
    EslZap* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event < EzEventSubmenu) return false;
    uint32_t index = event.event - EzEventSubmenu;
    scene_manager_set_scene_state(app->scene_manager, EzSceneTagMenu, index);
    EzTag* tag = ez_edit_tag(app);
    if(!tag) return false;

    switch(index) {
    case EzTagMenuUse:
        app->store.settings.active = app->edit_tag;
        ez_reset_render_defaults(app);
        ez_store_save_settings(&app->store, app->storage);
        ez_feedback(app, &sequence_single_vibro);
        if(!scene_manager_search_and_switch_to_previous_scene(app->scene_manager, EzSceneHome)) {
            scene_manager_search_and_switch_to_another_scene(app->scene_manager, EzSceneHome);
        }
        break;
    case EzTagMenuInfo:
        scene_manager_next_scene(app->scene_manager, EzSceneTagInfo);
        break;
    case EzTagMenuRename:
        strncpy(app->name_buf, tag->name, EZ_NAME_MAX);
        app->name_buf[EZ_NAME_MAX] = '\0';
        scene_manager_next_scene(app->scene_manager, EzSceneTagName);
        break;
    case EzTagMenuSize:
        scene_manager_next_scene(app->scene_manager, EzSceneTagSize);
        break;
    default:
        scene_manager_next_scene(app->scene_manager, EzSceneTagDelete);
        break;
    }
    return true;
}

void ez_scene_tag_menu_on_exit(void* context) {
    EslZap* app = context;
    submenu_reset(app->submenu);
}

/* ---- tag details --------------------------------------------------------- */

void ez_scene_tag_info_on_enter(void* context) {
    EslZap* app = context;
    EzTag* tag = ez_edit_tag(app);
    widget_reset(app->widget);
    if(tag) {
        EzModel model;
        bool graphic = ez_tag_model(tag, &model);
        uint8_t addr[4] = {0};
        ez_barcode_address(tag->barcode, addr);
        FuriString* s = app->scratch;
        furi_string_printf(
            s,
            "\e#%s\n%s: %s\n%s: %04u %s\n",
            tag->name,
            T(S_INFO_BARCODE),
            tag->barcode,
            T(S_INFO_MODEL),
            model.code,
            ez_model_find(model.code) ? model.name : T(S_INFO_UNKNOWN));
        if(graphic) {
            furi_string_cat_printf(
                s,
                "%s: %ux%u %s\n",
                T(S_INFO_PANEL),
                model.width,
                model.height,
                ez_accent_name(model.accent));
        } else if(model.kind == EzKindSegment) {
            furi_string_cat_printf(s, "%s: %s\n", T(S_INFO_PANEL), T(S_BC_SEGMENT));
        } else {
            furi_string_cat_printf(s, "%s: %s\n", T(S_INFO_PANEL), T(S_INFO_UNKNOWN));
        }
        furi_string_cat_printf(
            s,
            "%s: %02X %02X %02X %02X",
            T(S_INFO_ADDRESS),
            addr[0],
            addr[1],
            addr[2],
            addr[3]);
        widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(s));
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, EzViewWidget);
}

bool ez_scene_tag_info_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void ez_scene_tag_info_on_exit(void* context) {
    EslZap* app = context;
    widget_reset(app->widget);
}

/* ---- panel size for unknown models ---------------------------------------- */

enum {
    EzSizeItemSize,
    EzSizeItemColors,
    EzSizeItemSave,
};

static const EzString ez_color_names[3] = {S_C_MONO, S_C_RED, S_C_YELLOW};

static void ez_size_changed(VariableItem* item) {
    EslZap* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    uint8_t which = variable_item_list_get_selected_item_index(app->list);
    char text[12];
    if(which == EzSizeItemSize) {
        app->size_index = v;
        snprintf(text, sizeof(text), "%ux%u", ez_common_sizes[v].w, ez_common_sizes[v].h);
        variable_item_set_current_value_text(item, text);
    } else if(which == EzSizeItemColors) {
        app->color_index = v;
        variable_item_set_current_value_text(item, T(ez_color_names[v]));
    }
}

void ez_scene_tag_size_on_enter(void* context) {
    EslZap* app = context;
    EzTag* tag = ez_edit_tag(app);
    if(!tag) {
        scene_manager_previous_scene(app->scene_manager);
        return;
    }
    app->size_index = 4; /* 296x128 */
    for(uint8_t i = 0; i < ez_common_sizes_count; i++) {
        if(ez_common_sizes[i].w == tag->custom_w && ez_common_sizes[i].h == tag->custom_h)
            app->size_index = i;
    }
    app->color_index = (tag->custom_accent <= EzAccentYellow) ? tag->custom_accent : 0;

    VariableItemList* list = app->list;
    variable_item_list_reset(list);
    char text[12];
    VariableItem* item =
        variable_item_list_add(list, T(S_SIZE), ez_common_sizes_count, ez_size_changed, app);
    variable_item_set_current_value_index(item, app->size_index);
    snprintf(
        text,
        sizeof(text),
        "%ux%u",
        ez_common_sizes[app->size_index].w,
        ez_common_sizes[app->size_index].h);
    variable_item_set_current_value_text(item, text);

    item = variable_item_list_add(list, T(S_COLORS), 3, ez_size_changed, app);
    variable_item_set_current_value_index(item, app->color_index);
    variable_item_set_current_value_text(item, T(ez_color_names[app->color_index]));

    variable_item_list_add(list, T(S_SAVE), 0, NULL, app);
    variable_item_list_set_enter_callback(list, ez_list_enter_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, EzViewList);
}

bool ez_scene_tag_size_on_event(void* context, SceneManagerEvent event) {
    EslZap* app = context;
    if(event.type == SceneManagerEventTypeCustom &&
       event.event == EzEventListEnter + EzSizeItemSave) {
        EzTag* tag = ez_edit_tag(app);
        if(tag) {
            tag->custom_w = ez_common_sizes[app->size_index].w;
            tag->custom_h = ez_common_sizes[app->size_index].h;
            tag->custom_accent = app->color_index;
            ez_store_save_tags(&app->store, app->storage);
            if(app->edit_tag == app->store.settings.active) ez_reset_render_defaults(app);
            ez_feedback(app, &sequence_success);
        }
        ez_return_to_list(app);
        return true;
    }
    return false;
}

void ez_scene_tag_size_on_exit(void* context) {
    EslZap* app = context;
    variable_item_list_reset(app->list);
}

/* ---- delete -------------------------------------------------------------- */

void ez_scene_tag_delete_on_enter(void* context) {
    EslZap* app = context;
    EzTag* tag = ez_edit_tag(app);
    dialog_ex_reset(app->dialog);
    dialog_ex_set_header(app->dialog, T(S_DELETE_Q), 64, 2, AlignCenter, AlignTop);
    dialog_ex_set_text(app->dialog, tag ? tag->name : "", 64, 28, AlignCenter, AlignCenter);
    dialog_ex_set_left_button_text(app->dialog, T(S_CANCEL));
    dialog_ex_set_right_button_text(app->dialog, T(S_TAG_DELETE));
    dialog_ex_set_result_callback(app->dialog, ez_dialog_callback);
    dialog_ex_set_context(app->dialog, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, EzViewDialog);
}

bool ez_scene_tag_delete_on_event(void* context, SceneManagerEvent event) {
    EslZap* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event == EzEventDialogRight) {
        ez_store_remove_tag(&app->store, app->edit_tag);
        ez_store_save_tags(&app->store, app->storage);
        ez_store_save_settings(&app->store, app->storage);
        ez_reset_render_defaults(app);
        scene_manager_set_scene_state(app->scene_manager, EzSceneTags, 0);
        ez_return_to_list(app);
        return true;
    }
    if(event.event == EzEventDialogLeft) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void ez_scene_tag_delete_on_exit(void* context) {
    EslZap* app = context;
    dialog_ex_reset(app->dialog);
}

/* ---- barcode entry -------------------------------------------------------- */

void ez_scene_barcode_on_enter(void* context) {
    EslZap* app = context;
    view_dispatcher_switch_to_view(app->view_dispatcher, EzViewBarcode);
}

bool ez_scene_barcode_on_event(void* context, SceneManagerEvent event) {
    EslZap* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event != EzEventBarcodeDone)
        return false;

    char code[EZ_BARCODE_LEN + 1];
    ez_barcode_view_get(app->barcode_view, code);
    if(!ez_barcode_is_valid(code)) {
        ez_barcode_view_set_error(app->barcode_view, S_BC_INVALID);
        ez_feedback(app, &sequence_error);
        return true;
    }
    if(ez_store_find(&app->store, code) >= 0) {
        ez_barcode_view_set_error(app->barcode_view, S_BC_EXISTS);
        ez_feedback(app, &sequence_error);
        return true;
    }
    memcpy(app->barcode, code, sizeof(code));
    snprintf(app->name_buf, sizeof(app->name_buf), "%s %u", T(S_TAG_DEFAULT), app->store.tag_count + 1);
    scene_manager_next_scene(app->scene_manager, EzSceneTagName);
    return true;
}

void ez_scene_barcode_on_exit(void* context) {
    UNUSED(context);
}

/* ---- tag name ------------------------------------------------------------ */

static void ez_text_done(void* context) {
    EslZap* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, EzEventTextDone);
}

void ez_scene_tag_name_on_enter(void* context) {
    EslZap* app = context;
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, T(S_NAME_TITLE));
    text_input_set_result_callback(
        app->text_input,
        ez_text_done,
        app,
        app->name_buf,
        EZ_NAME_MAX + 1,
        app->edit_tag == EZ_NEW_TAG);
    view_dispatcher_switch_to_view(app->view_dispatcher, EzViewTextInput);
}

bool ez_scene_tag_name_on_event(void* context, SceneManagerEvent event) {
    EslZap* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event != EzEventTextDone) return false;

    for(char* c = app->name_buf; *c; c++) {
        if(*c == ';' || *c == '\n' || *c == '\r') *c = ' ';
    }
    if(!app->name_buf[0]) {
        snprintf(
            app->name_buf, sizeof(app->name_buf), "%s %u", T(S_TAG_DEFAULT), app->store.tag_count + 1);
    }

    if(app->edit_tag == EZ_NEW_TAG) {
        int index = ez_store_add_tag(&app->store, app->barcode, app->name_buf);
        if(index < 0) {
            ez_return_to_list(app);
            return true;
        }
        app->store.settings.active = (uint8_t)index;
        ez_store_save_tags(&app->store, app->storage);
        ez_store_save_settings(&app->store, app->storage);
        ez_reset_render_defaults(app);
        ez_feedback(app, &sequence_success);

        EzModel model;
        ez_tag_model(&app->store.tags[index], &model);
        if(model.kind == EzKindUnknown) {
            /* unknown model: ask for the panel size right away */
            app->edit_tag = (uint8_t)index;
            scene_manager_search_and_switch_to_another_scene(app->scene_manager, EzSceneTagSize);
        } else {
            ez_return_to_list(app);
        }
    } else {
        EzTag* tag = ez_edit_tag(app);
        if(tag) {
            strncpy(tag->name, app->name_buf, EZ_NAME_MAX);
            tag->name[EZ_NAME_MAX] = '\0';
            ez_store_save_tags(&app->store, app->storage);
        }
        scene_manager_previous_scene(app->scene_manager);
    }
    return true;
}

void ez_scene_tag_name_on_exit(void* context) {
    EslZap* app = context;
    text_input_reset(app->text_input);
}

/* ---- import from NFC (live read) ----------------------------------------- */

#define EZ_NFC_READ_ATTEMPTS 3

/* Runs on NFC threads: only forwards the event to the GUI thread. */
static void ez_nfc_reader_event(EzNfcReaderEvent event, void* context) {
    EslZap* app = context;
    uint32_t ev = EzEventNfcFailed;
    if(event == EzNfcReaderFound) ev = EzEventNfcFound;
    if(event == EzNfcReaderIncompatible) ev = EzEventNfcIncompatible;
    if(event == EzNfcReaderRead) ev = EzEventNfcRead;
    view_dispatcher_send_custom_event(app->view_dispatcher, ev);
}

/* Starts (or restarts) the search. It lasts until a chip shows up or the
 * user presses Cancel. */
static void ez_nfc_search(EslZap* app) {
    app->nfc_fails = 0;
    ez_nfc_view_searching(app->nfc_view);
    ez_nfc_reader_scan(app->nfc_reader);
    ez_feedback(app, &sequence_blink_start_cyan);
}

void ez_scene_nfc_import_on_enter(void* context) {
    EslZap* app = context;
    app->nfc_reader = ez_nfc_reader_alloc(ez_nfc_reader_event, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, EzViewNfc);
    ez_nfc_search(app);
}

static void ez_nfc_problem(EslZap* app, EzString title, EzString reason) {
    ez_nfc_view_problem(app->nfc_view, title, reason);
    ez_feedback(app, &sequence_error);
}

/* The chip was read: decode it and show the result. */
static void ez_nfc_handle_read(EslZap* app) {
    size_t len = 0;
    const uint8_t* mem = ez_nfc_reader_memory(app->nfc_reader, &len);
    char barcode[EZ_BARCODE_LEN + 1] = {0};
    EzNfcResult r = ez_nfc_parse_memory(mem, len, barcode, NULL, 0);

    switch(r) {
    case EzNfcOk: {
        if(ez_store_find(&app->store, barcode) >= 0) {
            ez_nfc_problem(app, S_NFC_FOUND, S_BC_EXISTS);
            return;
        }
        memcpy(app->barcode, barcode, sizeof(barcode));
        char line1[32];
        const EzModel* model = ez_model_find(ez_barcode_model(barcode));
        if(model && model->kind == EzKindGraphic)
            snprintf(line1, sizeof(line1), "%s %ux%u", model->name, model->width, model->height);
        else if(model)
            snprintf(line1, sizeof(line1), "%s", model->name);
        else
            snprintf(line1, sizeof(line1), "%s %04u", T(S_BC_UNKNOWN), ez_barcode_model(barcode));
        ez_nfc_view_found(app->nfc_view, line1, barcode);
        ez_feedback(app, &sequence_success);
        return;
    }
    case EzNfcRadioTag:
        ez_nfc_problem(app, S_NFC_INCOMPATIBLE, S_NFC_RADIO);
        return;
    case EzNfcBadCheck:
        ez_nfc_problem(app, S_NFC_INCOMPATIBLE, S_NFC_BADCHECK);
        return;
    case EzNfcUnknownId:
        ez_nfc_problem(app, S_NFC_INCOMPATIBLE, S_NFC_UNKNOWN);
        return;
    default:
        ez_nfc_problem(app, S_NFC_INCOMPATIBLE, S_NFC_NO_NDEF);
        return;
    }
}

bool ez_scene_nfc_import_on_event(void* context, SceneManagerEvent event) {
    EslZap* app = context;
    if(event.type == SceneManagerEventTypeTick) {
        ez_nfc_view_tick(app->nfc_view);
        return true;
    }
    if(event.type != SceneManagerEventTypeCustom) return false;

    switch(event.event) {
    case EzEventNfcFound:
        /* a label-type chip answered: read it */
        ez_nfc_reader_read(app->nfc_reader);
        return true;
    case EzEventNfcIncompatible:
        ez_nfc_reader_stop(app->nfc_reader);
        notification_message(app->notifications, &sequence_blink_stop);
        ez_nfc_problem(app, S_NFC_INCOMPATIBLE, S_NFC_NOT_ESL);
        return true;
    case EzEventNfcRead:
        ez_nfc_reader_stop(app->nfc_reader);
        notification_message(app->notifications, &sequence_blink_stop);
        ez_nfc_handle_read(app);
        return true;
    case EzEventNfcFailed:
        /* chip moved away or read error: search again, give up after a few */
        if(++app->nfc_fails < EZ_NFC_READ_ATTEMPTS) {
            ez_nfc_reader_scan(app->nfc_reader);
        } else {
            ez_nfc_reader_stop(app->nfc_reader);
            notification_message(app->notifications, &sequence_blink_stop);
            ez_nfc_problem(app, S_NFC_INCOMPATIBLE, S_NFC_READ_FAIL);
        }
        return true;
    case EzEventNfcRetry:
        ez_nfc_search(app);
        return true;
    case EzEventNfcCancel:
        scene_manager_previous_scene(app->scene_manager);
        return true;
    case EzEventNfcSave:
        app->edit_tag = EZ_NEW_TAG;
        snprintf(
            app->name_buf, sizeof(app->name_buf), "%s %u", T(S_TAG_DEFAULT), app->store.tag_count + 1);
        scene_manager_next_scene(app->scene_manager, EzSceneTagName);
        return true;
    default:
        return false;
    }
}

void ez_scene_nfc_import_on_exit(void* context) {
    EslZap* app = context;
    notification_message(app->notifications, &sequence_blink_stop);
    if(app->nfc_reader) {
        ez_nfc_reader_free(app->nfc_reader);
        app->nfc_reader = NULL;
    }
}
