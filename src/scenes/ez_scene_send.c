/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 *
 * Transmission, page switching and settings.
 */
#include "../esl_zap.h"

#include <stdio.h>
#include <string.h>

/* ---- transmission -------------------------------------------------------- */

static const uint16_t ez_page_seconds[] = {0, 2, 15, 900};
#define EZ_PAGE_DURATIONS (sizeof(ez_page_seconds) / sizeof(ez_page_seconds[0]))

static void ez_send_progress(const EzProgress* progress, void* context) {
    EslZap* app = context;
    ez_send_view_update(app->send_view, progress);
    app->last_phase = progress->phase;
    if(progress->phase == EzPhaseDone || progress->phase == EzPhaseError ||
       progress->phase == EzPhaseCancelled) {
        view_dispatcher_send_custom_event(app->view_dispatcher, EzEventSendFinished);
    }
}

static void ez_send_stop_worker(EslZap* app) {
    if(app->worker) {
        app->cancel = true;
        furi_thread_join(app->worker);
        furi_thread_free(app->worker);
        app->worker = NULL;
    }
}

static void ez_send_start(EslZap* app) {
    EzTag* tag = ez_active_tag(app);
    if(!tag) return;
    ez_send_stop_worker(app);

    EzJob* job = &app->job;
    memset(job, 0, sizeof(EzJob));
    job->kind = app->page_job ? EzJobPage : EzJobImage;
    ez_barcode_address(tag->barcode, job->address);
    ez_tag_model(tag, &job->model);
    job->spec = app->spec;
    job->page = app->page;
    job->page_seconds =
        ez_page_seconds[app->page_seconds_index < EZ_PAGE_DURATIONS ? app->page_seconds_index : 0];
    job->phy = (EzPhy)app->store.settings.phy;
    /* segment labels never understood PP16 */
    if(job->model.kind == EzKindSegment) job->phy = EzPhyPP4;
    job->output = (EzIrOutput)app->store.settings.output;
    job->wake_ms = ez_wake_ms(app->store.settings.wake);
    job->sends = app->store.settings.sends;
    job->storage = app->storage;
    job->cancel = &app->cancel;
    job->callback = ez_send_progress;
    job->context = app;

    app->cancel = false;
    app->last_phase = EzPhasePrepare;
    ez_send_view_reset(app->send_view, tag->name);

    app->worker = furi_thread_alloc_ex("EslZapTx", 4096, ez_job_thread, job);
    furi_thread_start(app->worker);
    ez_feedback(app, &sequence_blink_start_magenta);
}

void ez_scene_send_on_enter(void* context) {
    EslZap* app = context;
    view_dispatcher_switch_to_view(app->view_dispatcher, EzViewSend);
    ez_send_start(app);
}

bool ez_scene_send_on_event(void* context, SceneManagerEvent event) {
    EslZap* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case EzEventSendFinished:
            if(app->worker) {
                furi_thread_join(app->worker);
                furi_thread_free(app->worker);
                app->worker = NULL;
            }
            notification_message(app->notifications, &sequence_blink_stop);
            if(app->last_phase == EzPhaseDone)
                ez_feedback(app, &sequence_success);
            else if(app->last_phase == EzPhaseError)
                ez_feedback(app, &sequence_error);
            return true;
        case EzEventSendAgain:
            ez_send_start(app);
            return true;
        case EzEventSendHome:
            if(!scene_manager_search_and_switch_to_previous_scene(
                   app->scene_manager, EzSceneHome)) {
                scene_manager_search_and_switch_to_another_scene(app->scene_manager, EzSceneHome);
            }
            return true;
        default:
            return false;
        }
    }
    if(event.type == SceneManagerEventTypeBack) {
        if(app->worker) {
            /* first Back stops the transmission, the result screen follows */
            app->cancel = true;
            return true;
        }
    }
    return false;
}

void ez_scene_send_on_exit(void* context) {
    EslZap* app = context;
    ez_send_stop_worker(app);
    notification_message(app->notifications, &sequence_blink_stop);
}

/* ---- page switching ------------------------------------------------------ */

enum {
    EzPageItemPage,
    EzPageItemDuration,
    EzPageItemSend,
};

static void ez_page_changed(VariableItem* item) {
    EslZap* app = variable_item_get_context(item);
    uint8_t which = variable_item_list_get_selected_item_index(app->list);
    uint8_t v = variable_item_get_current_value_index(item);
    char text[8];
    if(which == EzPageItemPage) {
        app->page = v;
        snprintf(text, sizeof(text), "%u", v);
        variable_item_set_current_value_text(item, text);
    } else if(which == EzPageItemDuration) {
        app->page_seconds_index = v;
        static const char* const labels[] = {NULL, "2 s", "15 s", "15 min"};
        variable_item_set_current_value_text(item, v == 0 ? T(S_FOREVER) : labels[v]);
    }
}

void ez_scene_page_on_enter(void* context) {
    EslZap* app = context;
    VariableItemList* list = app->list;
    variable_item_list_reset(list);
    char text[8];

    VariableItem* item = variable_item_list_add(list, T(S_OPT_PAGE), 8, ez_page_changed, app);
    variable_item_set_current_value_index(item, app->page & 7U);
    snprintf(text, sizeof(text), "%u", app->page & 7U);
    variable_item_set_current_value_text(item, text);

    item = variable_item_list_add(list, T(S_PAGE_TIME), EZ_PAGE_DURATIONS, ez_page_changed, app);
    if(app->page_seconds_index >= EZ_PAGE_DURATIONS) app->page_seconds_index = 0;
    variable_item_set_current_value_index(item, app->page_seconds_index);
    static const char* const labels[] = {NULL, "2 s", "15 s", "15 min"};
    variable_item_set_current_value_text(
        item, app->page_seconds_index == 0 ? T(S_FOREVER) : labels[app->page_seconds_index]);

    variable_item_list_add(list, T(S_PAGE_SEND), 0, NULL, app);
    variable_item_list_set_enter_callback(list, ez_list_enter_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, EzViewList);
}

bool ez_scene_page_on_event(void* context, SceneManagerEvent event) {
    EslZap* app = context;
    if(event.type == SceneManagerEventTypeCustom &&
       event.event == EzEventListEnter + EzPageItemSend) {
        app->page_job = true;
        scene_manager_next_scene(app->scene_manager, EzSceneSend);
        return true;
    }
    return false;
}

void ez_scene_page_on_exit(void* context) {
    EslZap* app = context;
    variable_item_list_reset(app->list);
}

/* ---- settings ------------------------------------------------------------ */

enum {
    EzSetLanguage,
    EzSetPhy,
    EzSetWake,
    EzSetSends,
    EzSetOutput,
    EzSetFeedback,
    EzSetAbout,
};

static const char* const ez_wake_labels[EzWakeCount] = {"3 s", "8 s", "15 s"};
static const char* const ez_send_labels[3] = {"1x", "2x", "3x"};

static void ez_settings_changed(VariableItem* item) {
    EslZap* app = variable_item_get_context(item);
    uint8_t which = variable_item_list_get_selected_item_index(app->list);
    uint8_t v = variable_item_get_current_value_index(item);
    EzSettings* s = &app->store.settings;

    switch(which) {
    case EzSetLanguage:
        s->language = v;
        ez_i18n_set(v);
        variable_item_set_current_value_text(item, v ? "Italiano" : "English");
        /* labels of the other items change too */
        view_dispatcher_send_custom_event(app->view_dispatcher, EzEventRebuild);
        break;
    case EzSetPhy:
        s->phy = v;
        variable_item_set_current_value_text(item, v ? "PP16" : "PP4");
        break;
    case EzSetWake:
        s->wake = v;
        variable_item_set_current_value_text(item, ez_wake_labels[v]);
        break;
    case EzSetSends:
        s->sends = (uint8_t)(v + 1U);
        variable_item_set_current_value_text(item, ez_send_labels[v]);
        break;
    case EzSetOutput:
        s->output = v;
        variable_item_set_current_value_text(item, v ? T(S_OUT_EXT) : T(S_OUT_INT));
        break;
    case EzSetFeedback:
        s->feedback = v;
        variable_item_set_current_value_text(item, v ? T(S_ON) : T(S_OFF));
        break;
    default:
        break;
    }
    ez_store_save_settings(&app->store, app->storage);
}

static void ez_settings_build(EslZap* app) {
    VariableItemList* list = app->list;
    EzSettings* s = &app->store.settings;
    variable_item_list_reset(list);

    VariableItem* item =
        variable_item_list_add(list, T(S_SET_LANG), EzLangCount, ez_settings_changed, app);
    variable_item_set_current_value_index(item, s->language);
    variable_item_set_current_value_text(item, s->language ? "Italiano" : "English");

    item = variable_item_list_add(list, T(S_SET_PHY), 2, ez_settings_changed, app);
    variable_item_set_current_value_index(item, s->phy);
    variable_item_set_current_value_text(item, s->phy ? "PP16" : "PP4");

    item = variable_item_list_add(list, T(S_SET_WAKE), EzWakeCount, ez_settings_changed, app);
    variable_item_set_current_value_index(item, s->wake);
    variable_item_set_current_value_text(item, ez_wake_labels[s->wake]);

    item = variable_item_list_add(list, T(S_SET_SENDS), 3, ez_settings_changed, app);
    variable_item_set_current_value_index(item, (uint8_t)(s->sends - 1U));
    variable_item_set_current_value_text(item, ez_send_labels[s->sends - 1U]);

    item = variable_item_list_add(list, T(S_SET_OUTPUT), 2, ez_settings_changed, app);
    variable_item_set_current_value_index(item, s->output);
    variable_item_set_current_value_text(item, s->output ? T(S_OUT_EXT) : T(S_OUT_INT));

    item = variable_item_list_add(list, T(S_SET_FEEDBACK), 2, ez_settings_changed, app);
    variable_item_set_current_value_index(item, s->feedback);
    variable_item_set_current_value_text(item, s->feedback ? T(S_ON) : T(S_OFF));

    variable_item_list_add(list, T(S_SET_ABOUT), 0, NULL, app);
    variable_item_list_set_enter_callback(list, ez_list_enter_callback, app);
}

void ez_scene_settings_on_enter(void* context) {
    EslZap* app = context;
    ez_settings_build(app);
    variable_item_list_set_selected_item(
        app->list, scene_manager_get_scene_state(app->scene_manager, EzSceneSettings));
    view_dispatcher_switch_to_view(app->view_dispatcher, EzViewList);
}

bool ez_scene_settings_on_event(void* context, SceneManagerEvent event) {
    EslZap* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event == EzEventRebuild) {
        ez_settings_build(app);
        variable_item_list_set_selected_item(app->list, EzSetLanguage);
        return true;
    }
    if(event.event == EzEventListEnter + EzSetAbout) {
        scene_manager_set_scene_state(app->scene_manager, EzSceneSettings, EzSetAbout);
        scene_manager_next_scene(app->scene_manager, EzSceneAbout);
        return true;
    }
    return false;
}

void ez_scene_settings_on_exit(void* context) {
    EslZap* app = context;
    ez_store_save_settings(&app->store, app->storage);
    variable_item_list_reset(app->list);
}
