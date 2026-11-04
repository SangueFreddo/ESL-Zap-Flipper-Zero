/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 *
 * Welcome wizard, home, help, about and the generic notice popup.
 */
#include "../esl_zap.h"

#include <string.h>

void ez_submenu_callback(void* context, uint32_t index) {
    EslZap* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, EzEventSubmenu + index);
}

void ez_dialog_callback(DialogExResult result, void* context) {
    EslZap* app = context;
    uint32_t event = EzEventDialogCenter;
    if(result == DialogExResultLeft) event = EzEventDialogLeft;
    if(result == DialogExResultRight) event = EzEventDialogRight;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

void ez_list_enter_callback(void* context, uint32_t index) {
    EslZap* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, EzEventListEnter + index);
}

void ez_return_to_list(EslZap* app) {
    if(!scene_manager_search_and_switch_to_previous_scene(app->scene_manager, EzSceneTags)) {
        if(!scene_manager_search_and_switch_to_previous_scene(app->scene_manager, EzSceneHome)) {
            scene_manager_search_and_switch_to_another_scene(app->scene_manager, EzSceneHome);
        }
    }
}

/* ---- welcome ------------------------------------------------------------- */

static void ez_welcome_show_dialog(EslZap* app) {
    dialog_ex_reset(app->dialog);
    dialog_ex_set_header(app->dialog, T(S_WELCOME), 64, 1, AlignCenter, AlignTop);
    dialog_ex_set_text(app->dialog, T(S_WELCOME_TEXT), 64, 33, AlignCenter, AlignCenter);
    dialog_ex_set_center_button_text(app->dialog, T(S_START));
    dialog_ex_set_result_callback(app->dialog, ez_dialog_callback);
    dialog_ex_set_context(app->dialog, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, EzViewDialog);
}

void ez_scene_welcome_on_enter(void* context) {
    EslZap* app = context;
    if(scene_manager_get_scene_state(app->scene_manager, EzSceneWelcome) == 1) {
        ez_welcome_show_dialog(app);
        return;
    }
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Language / Lingua");
    submenu_add_item(app->submenu, "English", EzLangEnglish, ez_submenu_callback, app);
    submenu_add_item(app->submenu, "Italiano", EzLangItalian, ez_submenu_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, EzViewSubmenu);
}

bool ez_scene_welcome_on_event(void* context, SceneManagerEvent event) {
    EslZap* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event >= EzEventSubmenu) {
            app->store.settings.language = (uint8_t)(event.event - EzEventSubmenu);
            ez_i18n_set(app->store.settings.language);
            scene_manager_set_scene_state(app->scene_manager, EzSceneWelcome, 1);
            ez_welcome_show_dialog(app);
            return true;
        }
        if(event.event == EzEventDialogCenter) {
            app->store.settings.setup_done = true;
            ez_store_save_settings(&app->store, app->storage);
            scene_manager_search_and_switch_to_another_scene(app->scene_manager, EzSceneHome);
            return true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        if(scene_manager_get_scene_state(app->scene_manager, EzSceneWelcome) == 1) {
            scene_manager_set_scene_state(app->scene_manager, EzSceneWelcome, 0);
            ez_scene_welcome_on_enter(app);
            return true;
        }
    }
    return false;
}

void ez_scene_welcome_on_exit(void* context) {
    EslZap* app = context;
    submenu_reset(app->submenu);
    dialog_ex_reset(app->dialog);
}

/* ---- home ---------------------------------------------------------------- */

void ez_scene_home_on_enter(void* context) {
    EslZap* app = context;
    ez_home_refresh(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, EzViewHome);
}

bool ez_scene_home_on_event(void* context, SceneManagerEvent event) {
    EslZap* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == EzEventTagPrev || event.event == EzEventTagNext) {
        uint8_t n = app->store.tag_count;
        if(n > 1) {
            uint8_t a = app->store.settings.active;
            a = (event.event == EzEventTagNext) ? (uint8_t)((a + 1) % n) :
                                                  (uint8_t)((a + n - 1) % n);
            app->store.settings.active = a;
            ez_reset_render_defaults(app);
            ez_store_save_settings(&app->store, app->storage);
            ez_home_refresh(app);
            ez_feedback(app, &sequence_single_vibro);
        }
        return true;
    }

    if(event.event >= EzEventHomeSelect && event.event < EzEventHomeSelect + EzHomeCount) {
        uint8_t item = (uint8_t)(event.event - EzEventHomeSelect);
        app->home_index = item;
        switch(item) {
        case EzHomeImage:
            if(ez_require_graphic_tag(app, NULL)) {
                app->spec.kind = EzSourceBmp;
                scene_manager_set_scene_state(app->scene_manager, EzScenePickImage, 0);
                scene_manager_next_scene(app->scene_manager, EzScenePickImage);
            }
            break;
        case EzHomeText:
            if(ez_require_graphic_tag(app, NULL)) {
                scene_manager_next_scene(app->scene_manager, EzSceneText);
            }
            break;
        case EzHomeTest:
            if(ez_require_graphic_tag(app, NULL)) {
                app->spec.kind = EzSourceTestCard;
                scene_manager_next_scene(app->scene_manager, EzScenePreview);
            }
            break;
        case EzHomeTags:
            scene_manager_next_scene(app->scene_manager, EzSceneTags);
            break;
        case EzHomePage:
            if(!ez_active_tag(app))
                ez_show_notice(app, S_NEED_TAG, S_NO_TAG, EzNoticeAddTag);
            else
                scene_manager_next_scene(app->scene_manager, EzScenePage);
            break;
        case EzHomeSettings:
            scene_manager_next_scene(app->scene_manager, EzSceneSettings);
            break;
        default:
            scene_manager_next_scene(app->scene_manager, EzSceneHelp);
            break;
        }
        return true;
    }
    return false;
}

void ez_scene_home_on_exit(void* context) {
    UNUSED(context);
}

/* ---- help ---------------------------------------------------------------- */

void ez_scene_help_on_enter(void* context) {
    EslZap* app = context;
    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_text(app->text_box, T(S_HELP_TEXT));
    text_box_set_focus(app->text_box, TextBoxFocusStart);
    view_dispatcher_switch_to_view(app->view_dispatcher, EzViewTextBox);
}

bool ez_scene_help_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void ez_scene_help_on_exit(void* context) {
    EslZap* app = context;
    text_box_reset(app->text_box);
}

/* ---- about --------------------------------------------------------------- */

void ez_scene_about_on_enter(void* context) {
    EslZap* app = context;
    widget_reset(app->widget);
    furi_string_printf(app->scratch, "%s\n\nVersion %s", T(S_ABOUT_TEXT), EZ_VERSION);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(app->scratch));
    view_dispatcher_switch_to_view(app->view_dispatcher, EzViewWidget);
}

bool ez_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void ez_scene_about_on_exit(void* context) {
    EslZap* app = context;
    widget_reset(app->widget);
}

/* ---- notice -------------------------------------------------------------- */

void ez_scene_notice_on_enter(void* context) {
    EslZap* app = context;
    dialog_ex_reset(app->dialog);
    dialog_ex_set_header(app->dialog, T(app->notice_header), 64, 2, AlignCenter, AlignTop);
    dialog_ex_set_text(app->dialog, T(app->notice_text), 64, 30, AlignCenter, AlignCenter);
    dialog_ex_set_center_button_text(app->dialog, "OK");
    dialog_ex_set_result_callback(app->dialog, ez_dialog_callback);
    dialog_ex_set_context(app->dialog, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, EzViewDialog);
}

bool ez_scene_notice_on_event(void* context, SceneManagerEvent event) {
    EslZap* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == EzEventDialogCenter) {
        switch(app->notice_action) {
        case EzNoticeAddTag:
            /* the tag list offers both barcode entry and NFC import */
            scene_manager_search_and_switch_to_another_scene(app->scene_manager, EzSceneTags);
            break;
        case EzNoticeTagSize:
            scene_manager_search_and_switch_to_another_scene(app->scene_manager, EzSceneTagSize);
            break;
        default:
            scene_manager_previous_scene(app->scene_manager);
            break;
        }
        return true;
    }
    return false;
}

void ez_scene_notice_on_exit(void* context) {
    EslZap* app = context;
    dialog_ex_reset(app->dialog);
}
