/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 *
 * Saved labels and settings, kept as small text files on the SD card:
 *   /ext/apps_data/esl_zap/tags.txt
 *   /ext/apps_data/esl_zap/settings.txt
 * Pictures live in /ext/apps_data/esl_zap/images
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <storage/storage.h>

#include "../proto/ez_proto.h"
#include "../proto/ez_models.h"

#define EZ_DIR EXT_PATH("apps_data/esl_zap")
#define EZ_IMAGES_DIR EXT_PATH("apps_data/esl_zap/images")
#define EZ_TAGS_FILE EXT_PATH("apps_data/esl_zap/tags.txt")
#define EZ_SETTINGS_FILE EXT_PATH("apps_data/esl_zap/settings.txt")
#define EZ_README_FILE EXT_PATH("apps_data/esl_zap/images/README.txt")

#define EZ_MAX_TAGS 24
#define EZ_NAME_MAX 20

typedef struct {
    char barcode[EZ_BARCODE_LEN + 1];
    char name[EZ_NAME_MAX + 1];
    /* only used when the model code is not in the built-in table */
    uint16_t custom_w;
    uint16_t custom_h;
    uint8_t custom_accent;
} EzTag;

typedef enum {
    EzLangEnglish = 0,
    EzLangItalian,
    EzLangCount,
} EzLang;

typedef enum {
    EzWakeShort = 0,
    EzWakeNormal,
    EzWakeLong,
    EzWakeCount,
} EzWake;

typedef struct {
    uint8_t language;
    uint8_t phy; /* EzPhy */
    uint8_t wake; /* EzWake */
    uint8_t sends; /* 1..3 transmissions of every data frame */
    uint8_t output; /* EzIrOutput */
    bool feedback; /* LED + vibration */
    bool setup_done; /* first run wizard completed */
    uint8_t active; /* index of the active tag */
} EzSettings;

typedef struct {
    EzSettings settings;
    EzTag tags[EZ_MAX_TAGS];
    uint8_t tag_count;
} EzStore;

void ez_store_defaults(EzStore* store);

/* Creates the folders (and the README in the images folder). */
void ez_store_prepare_folders(Storage* storage);

void ez_store_load(EzStore* store, Storage* storage);
bool ez_store_save_settings(const EzStore* store, Storage* storage);
bool ez_store_save_tags(const EzStore* store, Storage* storage);

/* Returns the index of the new tag, or -1 when full / duplicate. */
int ez_store_add_tag(EzStore* store, const char* barcode, const char* name);
void ez_store_remove_tag(EzStore* store, uint8_t index);
int ez_store_find(const EzStore* store, const char* barcode);

/* Resolves the panel model of a saved tag. Returns false when the size
 * is unknown (unknown model code and no custom size chosen). */
bool ez_tag_model(const EzTag* tag, EzModel* out);

uint16_t ez_wake_ms(uint8_t wake);
