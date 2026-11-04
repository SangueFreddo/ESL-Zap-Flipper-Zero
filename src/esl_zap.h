/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 */
#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_box.h>
#include <gui/modules/dialog_ex.h>
#include <dialogs/dialogs.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

#include "app/ez_store.h"
#include "app/ez_job.h"
#include "app/ez_i18n.h"
#include "image/ez_source.h"
#include "scenes/ez_scene.h"
#include "views/ez_views.h"

#define EZ_VERSION "1.0"
#define EZ_NEW_TAG 0xFF

typedef enum {
    EzViewSubmenu,
    EzViewList,
    EzViewTextInput,
    EzViewWidget,
    EzViewTextBox,
    EzViewDialog,
    EzViewHome,
    EzViewBarcode,
    EzViewPreview,
    EzViewSend,
    EzViewNfc,
} EzViewId;

typedef enum {
    EzEventHomeSelect = 100, /* + item index */
    EzEventTagPrev = 200,
    EzEventTagNext,
    EzEventBarcodeDone,
    EzEventPreviewSend,
    EzEventPreviewOptions,
    EzEventSendFinished,
    EzEventSendAgain,
    EzEventSendHome,
    EzEventTextDone,
    EzEventDialogLeft,
    EzEventDialogCenter,
    EzEventDialogRight,
    EzEventRebuild,
    EzEventNfcFound,
    EzEventNfcIncompatible,
    EzEventNfcRead,
    EzEventNfcFailed,
    EzEventNfcCancel,
    EzEventNfcSave,
    EzEventNfcRetry,
    EzEventListEnter = 300, /* + item index */
    EzEventSubmenu = 1000, /* + item index */
} EzEvent;

typedef enum {
    EzNoticeBack = 0,
    EzNoticeAddTag,
    EzNoticeTagSize,
} EzNoticeAction;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;
    Storage* storage;
    DialogsApp* dialogs;

    Submenu* submenu;
    VariableItemList* list;
    TextInput* text_input;
    Widget* widget;
    TextBox* text_box;
    DialogEx* dialog;
    View* home_view;
    View* barcode_view;
    View* preview_view;
    View* send_view;
    View* nfc_view;
    struct EzNfcReader* nfc_reader;
    uint8_t nfc_fails;

    EzStore store;

    /* working state */
    uint8_t home_index;
    uint8_t edit_tag; /* tag being edited, or EZ_NEW_TAG */
    char barcode[EZ_BARCODE_LEN + 1];
    char name_buf[EZ_NAME_MAX + 1];
    char text_buf[EZ_TEXT_MAX + 1];
    FuriString* image_path;
    FuriString* scratch;

    EzRenderSpec spec;
    uint8_t page;
    uint8_t page_seconds_index;
    bool page_job;

    /* option lists */
    uint8_t opt_types[8];
    uint8_t opt_count;
    uint8_t size_index;
    uint8_t color_index;

    /* notice popup */
    EzString notice_header;
    EzString notice_text;
    EzNoticeAction notice_action;

    /* transmission */
    EzJob job;
    FuriThread* worker;
    volatile bool cancel;
    EzPhase last_phase;
} EslZap;

/* Active tag or NULL. */
EzTag* ez_active_tag(EslZap* app);

/* Sets the default page and accent for the active tag. */
void ez_reset_render_defaults(EslZap* app);

/* Shows a message; `action` decides where OK leads. */
void ez_show_notice(EslZap* app, EzString header, EzString text, EzNoticeAction action);

void ez_feedback(EslZap* app, const NotificationSequence* seq);

/* Refreshes the home view after tag or language changes. */
void ez_home_refresh(EslZap* app);

/* Checks that the active tag can show pictures. Shows a notice if not. */
bool ez_require_graphic_tag(EslZap* app, EzModel* model);

/* Shared module callbacks (scenes/ez_scene_start.c) */
void ez_submenu_callback(void* context, uint32_t index);
void ez_dialog_callback(DialogExResult result, void* context);
void ez_list_enter_callback(void* context, uint32_t index);

/* Goes back to the tag list, or home when the list is not on the stack. */
void ez_return_to_list(EslZap* app);
