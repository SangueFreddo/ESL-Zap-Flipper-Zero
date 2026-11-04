/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 */
#include "ez_i18n.h"

static const char* const ez_strings_en[S_COUNT] = {
#define EZ_STR_EN(id, en, it) [id] = en,
    EZ_STRINGS(EZ_STR_EN)
#undef EZ_STR_EN
};

static const char* const ez_strings_it[S_COUNT] = {
#define EZ_STR_IT(id, en, it) [id] = it,
    EZ_STRINGS(EZ_STR_IT)
#undef EZ_STR_IT
};

static uint8_t ez_language = 0;

void ez_i18n_set(uint8_t language) {
    ez_language = language;
}

const char* T(EzString id) {
    if(id >= S_COUNT) return "";
    return (ez_language == 1) ? ez_strings_it[id] : ez_strings_en[id];
}
