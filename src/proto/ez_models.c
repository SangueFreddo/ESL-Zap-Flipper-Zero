/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 *
 * Model codes and panel sizes collected from public ESL research and
 * community testing.
 */
#include "ez_models.h"

#include <stddef.h>

#define G EzKindGraphic
#define S EzKindSegment
#define MONO EzAccentNone
#define RED EzAccentRed
#define YEL EzAccentYellow

static const EzModel ez_models[] = {
    /* segment labels: page switching only */
    {1206, 0, 0, S, MONO, 0, "Continuum E2 HCS"},
    {1207, 0, 0, S, MONO, 0, "Continuum E2 HCN"},
    {1217, 0, 0, S, MONO, 0, "Continuum E5 HCS"},
    {1219, 0, 0, S, MONO, 0, "Continuum E5 HCN"},
    {1240, 0, 0, S, MONO, 0, "Continuum E4 HCS"},
    {1241, 0, 0, S, MONO, 0, "Continuum E4 HCN"},
    {1242, 0, 0, S, MONO, 0, "Continuum E4 HCN FZ"},
    {1243, 0, 0, S, MONO, 0, "Continuum E4 HCW"},
    {1265, 0, 0, S, MONO, 0, "Continuum E5 HCS"},
    {1291, 0, 0, S, MONO, 0, "Promoline 3-16"},
    {1510, 0, 0, S, MONO, 0, "SmartTag E5 M"},

    /* graphic labels */
    {1275, 320, 192, G, MONO, 0, "DM110"},
    {1276, 320, 140, G, MONO, 0, "DM90"},
    {1300, 172, 72, G, MONO, 0, "DM3370"},
    {1314, 400, 300, G, MONO, 0, "HD110"},
    {1315, 296, 128, G, MONO, 0, "HD L"},
    {1317, 152, 152, G, MONO, 0, "HD S"},
    {1318, 208, 112, G, MONO, 0, "HD M"},
    {1319, 800, 480, G, MONO, 0, "HD200"},
    {1322, 152, 152, G, MONO, 0, "HD S"},
    {1324, 208, 112, G, MONO, 0, "HD M FZ"},
    {1327, 208, 112, G, RED, 0, "HD M Red"},
    {1328, 296, 128, G, RED, 0, "HD L Red"},
    {1336, 400, 300, G, RED, 0, "HD110 Red"},
    {1339, 152, 152, G, RED, 0, "HD S Red"},
    {1340, 800, 480, G, RED, 0, "HD200 Red"},
    {1344, 296, 128, G, YEL, 0, "HD L Yellow"},
    {1346, 800, 480, G, YEL, 0, "HD200 Yellow"},
    {1348, 264, 176, G, RED, 0, "HD T Red"},
    {1349, 264, 176, G, YEL, 0, "HD T Yellow"},
    {1351, 648, 480, G, MONO, 0, "HD150"},
    {1353, 648, 480, G, RED, 0, "HD150 Red"},
    {1354, 648, 480, G, RED, 0, "HD150 Red"},
    {1370, 296, 128, G, RED, 0, "HD L Red 2021"},
    {1371, 648, 480, G, RED, 0, "HD150 Red 2021"},
    {1626, 296, 152, G, RED, EZ_MODEL_ROTATED | EZ_MODEL_PAGE2, "Color 2.6"},
    {1627, 296, 128, G, RED, 0, "HD L Red"},
    {1628, 296, 128, G, RED, 0, "HD L Red"},
    {1639, 152, 152, G, RED, 0, "HD S Red"},
};

const EzSize ez_common_sizes[] = {
    {152, 152},
    {172, 72},
    {208, 112},
    {264, 176},
    {296, 128},
    {296, 152},
    {320, 140},
    {320, 192},
    {400, 300},
    {648, 480},
    {800, 480},
};

const uint8_t ez_common_sizes_count = sizeof(ez_common_sizes) / sizeof(ez_common_sizes[0]);

const EzModel* ez_model_find(uint16_t code) {
    for(size_t i = 0; i < sizeof(ez_models) / sizeof(ez_models[0]); i++) {
        if(ez_models[i].code == code) return &ez_models[i];
    }
    return NULL;
}

void ez_model_custom(EzModel* out, uint16_t code, uint16_t w, uint16_t h, EzAccent accent) {
    out->code = code;
    out->width = w;
    out->height = h;
    out->kind = (w && h) ? EzKindGraphic : EzKindUnknown;
    out->accent = accent;
    out->flags = 0;
    out->name = "Custom";
}

const char* ez_accent_name(EzAccent accent) {
    switch(accent) {
    case EzAccentRed:
        return "B/W/R";
    case EzAccentYellow:
        return "B/W/Y";
    default:
        return "B/W";
    }
}
