/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 *
 * Application entry point and shared helpers.
 */
#include "esl_zap.h"

#include <stdio.h>
#include <string.h>

static bool ez_custom_event(void* context, uint32_t event) {
    EslZap* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool ez_back_event(void* context) {
    EslZap* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void ez_tick_event(void* context) {
    EslZap* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

static void ez_view_event(uint32_t event, void* context) {
    EslZap* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

EzTag* ez_active_tag(EslZap* app) {
    if(app->store.tag_count == 0) return NULL;
    if(app->store.settings.active >= app->store.tag_count) app->store.settings.active = 0;
    return &app->store.tags[app->store.settings.active];
}

void ez_reset_render_defaults(EslZap* app) {
    EzTag* tag = ez_active_tag(app);
    EzModel model;
    memset(&model, 0, sizeof(model));
    if(tag) ez_tag_model(tag, &model);
    app->page = (model.flags & EZ_MODEL_PAGE2) ? 2 : 0;
    app->spec.accent = model.accent;
    app->spec.fit = EzFitContain;
    app->spec.dither = true;
    app->spec.invert = false;
    app->spec.frame = false;
    app->spec.accent_text = false;
}

void ez_show_notice(EslZap* app, EzString header, EzString text, EzNoticeAction action) {
    app->notice_header = header;
    app->notice_text = text;
    app->notice_action = action;
    scene_manager_next_scene(app->scene_manager, EzSceneNotice);
}

void ez_feedback(EslZap* app, const NotificationSequence* seq) {
    if(app->store.settings.feedback) notification_message(app->notifications, seq);
}

void ez_home_refresh(EslZap* app) {
    EzTag* tag = ez_active_tag(app);
    char info[16] = {0};
    if(tag) {
        EzModel model;
        bool graphic = ez_tag_model(tag, &model);
        if(graphic)
            snprintf(info, sizeof(info), "%ux%u", model.width, model.height);
        else if(model.kind == EzKindSegment)
            snprintf(info, sizeof(info), "SEG");
        else
            snprintf(info, sizeof(info), "?x?");
    }
    ez_home_view_set(
        app->home_view,
        app->home_index,
        tag ? tag->name : NULL,
        info,
        app->store.tag_count > 1);
}

bool ez_require_graphic_tag(EslZap* app, EzModel* model) {
    EzTag* tag = ez_active_tag(app);
    if(!tag) {
        ez_show_notice(app, S_NEED_TAG, S_NO_TAG, EzNoticeAddTag);
        return false;
    }
    EzModel m;
    bool graphic = ez_tag_model(tag, &m);
    if(!graphic) {
        if(m.kind == EzKindSegment) {
            ez_show_notice(app, S_HOME_PAGE, S_SEGMENT_ONLY, EzNoticeBack);
        } else {
            app->edit_tag = app->store.settings.active;
            ez_show_notice(app, S_TAG_SIZE, S_NEED_SIZE, EzNoticeTagSize);
        }
        return false;
    }
    if(model) *model = m;
    return true;
}

static EslZap* ez_alloc(void) {
    EslZap* app = malloc(sizeof(EslZap));
    memset(app, 0, sizeof(EslZap));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->image_path = furi_string_alloc_set_str(EZ_IMAGES_DIR);
    app->scratch = furi_string_alloc();

    ez_store_prepare_folders(app->storage);
    ez_store_load(&app->store, app->storage);
    ez_i18n_set(app->store.settings.language);
    ez_reset_render_defaults(app);
    app->edit_tag = EZ_NEW_TAG;

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&ez_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, ez_custom_event);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, ez_back_event);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, ez_tick_event, 120);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, EzViewSubmenu, submenu_get_view(app->submenu));
    app->list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, EzViewList, variable_item_list_get_view(app->list));
    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, EzViewTextInput, text_input_get_view(app->text_input));
    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, EzViewWidget, widget_get_view(app->widget));
    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, EzViewTextBox, text_box_get_view(app->text_box));
    app->dialog = dialog_ex_alloc();
    view_dispatcher_add_view(app->view_dispatcher, EzViewDialog, dialog_ex_get_view(app->dialog));

    app->home_view = ez_home_view_alloc();
    ez_home_view_set_callback(
        app->home_view, ez_view_event, app, EzEventHomeSelect, EzEventTagPrev, EzEventTagNext);
    view_dispatcher_add_view(app->view_dispatcher, EzViewHome, app->home_view);

    app->barcode_view = ez_barcode_view_alloc();
    ez_barcode_view_set_callback(app->barcode_view, ez_view_event, app, EzEventBarcodeDone);
    view_dispatcher_add_view(app->view_dispatcher, EzViewBarcode, app->barcode_view);

    app->preview_view = ez_preview_view_alloc();
    ez_preview_view_set_callback(
        app->preview_view, ez_view_event, app, EzEventPreviewSend, EzEventPreviewOptions);
    view_dispatcher_add_view(app->view_dispatcher, EzViewPreview, app->preview_view);

    app->send_view = ez_send_view_alloc();
    ez_send_view_set_callback(app->send_view, ez_view_event, app, EzEventSendAgain, EzEventSendHome);
    view_dispatcher_add_view(app->view_dispatcher, EzViewSend, app->send_view);

    app->nfc_view = ez_nfc_view_alloc();
    ez_nfc_view_set_callback(
        app->nfc_view, ez_view_event, app, EzEventNfcCancel, EzEventNfcSave, EzEventNfcRetry);
    view_dispatcher_add_view(app->view_dispatcher, EzViewNfc, app->nfc_view);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    return app;
}

static void ez_free(EslZap* app) {
    if(app->worker) {
        app->cancel = true;
        furi_thread_join(app->worker);
        furi_thread_free(app->worker);
        app->worker = NULL;
    }

    view_dispatcher_remove_view(app->view_dispatcher, EzViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, EzViewList);
    view_dispatcher_remove_view(app->view_dispatcher, EzViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, EzViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, EzViewTextBox);
    view_dispatcher_remove_view(app->view_dispatcher, EzViewDialog);
    view_dispatcher_remove_view(app->view_dispatcher, EzViewHome);
    view_dispatcher_remove_view(app->view_dispatcher, EzViewBarcode);
    view_dispatcher_remove_view(app->view_dispatcher, EzViewPreview);
    view_dispatcher_remove_view(app->view_dispatcher, EzViewSend);
    view_dispatcher_remove_view(app->view_dispatcher, EzViewNfc);

    submenu_free(app->submenu);
    variable_item_list_free(app->list);
    text_input_free(app->text_input);
    widget_free(app->widget);
    text_box_free(app->text_box);
    dialog_ex_free(app->dialog);
    ez_home_view_free(app->home_view);
    ez_barcode_view_free(app->barcode_view);
    ez_preview_view_free(app->preview_view);
    ez_send_view_free(app->send_view);
    ez_nfc_view_free(app->nfc_view);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    ez_store_save_settings(&app->store, app->storage);

    furi_string_free(app->image_path);
    furi_string_free(app->scratch);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t esl_zap_app(void* p) {
    UNUSED(p);
    EslZap* app = ez_alloc();
    scene_manager_next_scene(
        app->scene_manager, app->store.settings.setup_done ? EzSceneHome : EzSceneWelcome);
    view_dispatcher_run(app->view_dispatcher);
    ez_free(app);
    return 0;
}
