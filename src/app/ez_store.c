/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 */
#include "ez_store.h"
#include "../ir/ez_ir.h"

#include <furi.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define EZ_FILE_MAX 4096

void ez_store_defaults(EzStore* store) {
    memset(store, 0, sizeof(*store));
    store->settings.language = EzLangEnglish;
    store->settings.phy = EzPhyPP4;
    store->settings.wake = EzWakeNormal;
    store->settings.sends = 2;
    store->settings.output = EzIrOutputInternal;
    store->settings.feedback = true;
    store->settings.setup_done = false;
    store->settings.active = 0;
}

static const char ez_readme[] =
    "ESL Zap - images folder\n"
    "=======================\n\n"
    "Put your pictures here as .bmp files.\n"
    "Best results: use the ESL Zap web tool, it converts any photo\n"
    "to the exact size and colours of your label.\n\n"
    "Supported: BMP 1, 4, 8, 24 and 32 bit, any size up to 2048 px.\n"
    "ESL Zap scales, dithers and detects red/yellow automatically.\n\n"
    "Metti qui le tue immagini in formato .bmp.\n"
    "Per risultati migliori usa lo strumento web di ESL Zap.\n";

void ez_store_prepare_folders(Storage* storage) {
    storage_simply_mkdir(storage, EXT_PATH("apps_data"));
    storage_simply_mkdir(storage, EZ_DIR);
    storage_simply_mkdir(storage, EZ_IMAGES_DIR);
    if(!storage_file_exists(storage, EZ_README_FILE)) {
        File* f = storage_file_alloc(storage);
        if(storage_file_open(f, EZ_README_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            storage_file_write(f, ez_readme, strlen(ez_readme));
        }
        storage_file_close(f);
        storage_file_free(f);
    }
}

/* Reads a whole small text file into a heap buffer (NUL terminated). */
static char* ez_read_text(Storage* storage, const char* path) {
    File* f = storage_file_alloc(storage);
    char* buf = NULL;
    if(storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        buf = malloc(EZ_FILE_MAX + 1);
        size_t n = storage_file_read(f, buf, EZ_FILE_MAX);
        buf[n] = '\0';
    }
    storage_file_close(f);
    storage_file_free(f);
    return buf;
}

static bool ez_write_text(Storage* storage, const char* path, const char* text) {
    File* f = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        size_t len = strlen(text);
        ok = storage_file_write(f, text, len) == len;
    }
    storage_file_close(f);
    storage_file_free(f);
    return ok;
}

/* Iterates lines in place: returns the next line and advances *cursor. */
static char* ez_next_line(char** cursor) {
    char* s = *cursor;
    if(!s || !*s) return NULL;
    char* e = s;
    while(*e && *e != '\n')
        e++;
    if(*e) {
        *e = '\0';
        *cursor = e + 1;
    } else {
        *cursor = e;
    }
    size_t len = strlen(s);
    if(len && s[len - 1] == '\r') s[len - 1] = '\0';
    return s;
}

static void ez_parse_settings(EzStore* store, char* text) {
    char* cursor = text;
    char* line;
    EzSettings* s = &store->settings;
    while((line = ez_next_line(&cursor))) {
        char* eq = strchr(line, '=');
        if(!eq) continue;
        *eq = '\0';
        int v = atoi(eq + 1);
        if(!strcmp(line, "language") && v >= 0 && v < EzLangCount)
            s->language = (uint8_t)v;
        else if(!strcmp(line, "phy") && (v == EzPhyPP4 || v == EzPhyPP16))
            s->phy = (uint8_t)v;
        else if(!strcmp(line, "wake") && v >= 0 && v < EzWakeCount)
            s->wake = (uint8_t)v;
        else if(!strcmp(line, "sends") && v >= 1 && v <= 3)
            s->sends = (uint8_t)v;
        else if(!strcmp(line, "output") && v >= 0 && v <= 1)
            s->output = (uint8_t)v;
        else if(!strcmp(line, "feedback"))
            s->feedback = v != 0;
        else if(!strcmp(line, "setup"))
            s->setup_done = v != 0;
        else if(!strcmp(line, "active") && v >= 0 && v < EZ_MAX_TAGS)
            s->active = (uint8_t)v;
    }
}

/* Line format: BARCODE;custom_w;custom_h;custom_accent;name */
static void ez_parse_tags(EzStore* store, char* text) {
    char* cursor = text;
    char* line;
    store->tag_count = 0;
    while((line = ez_next_line(&cursor)) && store->tag_count < EZ_MAX_TAGS) {
        if(line[0] == '#' || !line[0]) continue;
        char* fields[5] = {0};
        uint8_t n = 0;
        char* p = line;
        fields[n++] = p;
        while(*p && n < 5) {
            if(*p == ';') {
                *p = '\0';
                fields[n++] = p + 1;
            }
            p++;
        }
        if(n < 5 || !ez_barcode_is_valid(fields[0])) continue;
        EzTag* tag = &store->tags[store->tag_count];
        memset(tag, 0, sizeof(*tag));
        strncpy(tag->barcode, fields[0], EZ_BARCODE_LEN);
        tag->barcode[0] = (char)((tag->barcode[0] >= 'a') ? tag->barcode[0] - 32 : tag->barcode[0]);
        tag->custom_w = (uint16_t)atoi(fields[1]);
        tag->custom_h = (uint16_t)atoi(fields[2]);
        tag->custom_accent = (uint8_t)atoi(fields[3]);
        if(tag->custom_accent > EzAccentYellow) tag->custom_accent = EzAccentNone;
        strncpy(tag->name, fields[4], EZ_NAME_MAX);
        if(!tag->name[0]) strncpy(tag->name, tag->barcode, EZ_NAME_MAX);
        store->tag_count++;
    }
}

void ez_store_load(EzStore* store, Storage* storage) {
    ez_store_defaults(store);
    char* text = ez_read_text(storage, EZ_SETTINGS_FILE);
    if(text) {
        ez_parse_settings(store, text);
        free(text);
    }
    text = ez_read_text(storage, EZ_TAGS_FILE);
    if(text) {
        ez_parse_tags(store, text);
        free(text);
    }
    if(store->settings.active >= store->tag_count) store->settings.active = 0;
}

bool ez_store_save_settings(const EzStore* store, Storage* storage) {
    char buf[200];
    const EzSettings* s = &store->settings;
    snprintf(
        buf,
        sizeof(buf),
        "# ESL Zap settings\nlanguage=%u\nphy=%u\nwake=%u\nsends=%u\noutput=%u\n"
        "feedback=%u\nsetup=%u\nactive=%u\n",
        s->language,
        s->phy,
        s->wake,
        s->sends,
        s->output,
        s->feedback ? 1U : 0U,
        s->setup_done ? 1U : 0U,
        s->active);
    return ez_write_text(storage, EZ_SETTINGS_FILE, buf);
}

bool ez_store_save_tags(const EzStore* store, Storage* storage) {
    FuriString* out = furi_string_alloc_set_str("# ESL Zap tags: barcode;width;height;accent;name\n");
    for(uint8_t i = 0; i < store->tag_count; i++) {
        const EzTag* t = &store->tags[i];
        /* names never contain ';' or newlines (filtered on input) */
        furi_string_cat_printf(
            out,
            "%s;%u;%u;%u;%s\n",
            t->barcode,
            t->custom_w,
            t->custom_h,
            t->custom_accent,
            t->name);
    }
    bool ok = ez_write_text(storage, EZ_TAGS_FILE, furi_string_get_cstr(out));
    furi_string_free(out);
    return ok;
}

int ez_store_find(const EzStore* store, const char* barcode) {
    for(uint8_t i = 0; i < store->tag_count; i++) {
        if(ez_barcode_same_label(store->tags[i].barcode, barcode)) return i;
    }
    return -1;
}

int ez_store_add_tag(EzStore* store, const char* barcode, const char* name) {
    if(store->tag_count >= EZ_MAX_TAGS) return -1;
    if(ez_store_find(store, barcode) >= 0) return -1;
    EzTag* tag = &store->tags[store->tag_count];
    memset(tag, 0, sizeof(*tag));
    strncpy(tag->barcode, barcode, EZ_BARCODE_LEN);
    strncpy(tag->name, name, EZ_NAME_MAX);
    for(char* c = tag->name; *c; c++) {
        if(*c == ';' || *c == '\n' || *c == '\r') *c = ' ';
    }
    return store->tag_count++;
}

void ez_store_remove_tag(EzStore* store, uint8_t index) {
    if(index >= store->tag_count) return;
    for(uint8_t i = index; i + 1 < store->tag_count; i++) {
        store->tags[i] = store->tags[i + 1];
    }
    store->tag_count--;
    if(store->settings.active >= store->tag_count) store->settings.active = 0;
    else if(store->settings.active > index) store->settings.active--;
}

bool ez_tag_model(const EzTag* tag, EzModel* out) {
    uint16_t code = ez_barcode_model(tag->barcode);
    const EzModel* known = ez_model_find(code);
    if(known) {
        *out = *known;
        return known->kind == EzKindGraphic;
    }
    ez_model_custom(out, code, tag->custom_w, tag->custom_h, (EzAccent)tag->custom_accent);
    return out->kind == EzKindGraphic;
}

uint16_t ez_wake_ms(uint8_t wake) {
    switch(wake) {
    case EzWakeShort:
        return 3000;
    case EzWakeLong:
        return 15000;
    default:
        return 8000;
    }
}
