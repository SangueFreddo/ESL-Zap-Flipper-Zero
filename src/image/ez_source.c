/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 */
#include "ez_source.h"
#include "ez_font.h"

#include <furi.h>
#include <string.h>
#include <stdlib.h>

#define EZ_BMP_MAX_SIDE 2048U
#define EZ_NO_COLUMN 0xFFFFU

/* ======================================================================== */
/* Colour classification and dithering (shared by all decoders)             */
/* ======================================================================== */

static bool ez_is_accent(uint8_t r, uint8_t g, uint8_t b, EzAccent accent) {
    int ri = r, gi = g, bi = b;
    if(accent == EzAccentRed) {
        return ri >= 110 && (ri - gi) >= 60 && (ri - bi) >= 60;
    }
    if(accent == EzAccentYellow) {
        int warm = (ri + gi) / 2;
        return ri >= 140 && gi >= 120 && (warm - bi) >= 70 && abs(ri - gi) <= 90;
    }
    return false;
}

static inline uint8_t ez_luma(uint8_t r, uint8_t g, uint8_t b) {
    return (uint8_t)((r * 77U + g * 150U + b * 29U) >> 8);
}

/* ======================================================================== */
/* BMP decoder                                                              */
/* ======================================================================== */

typedef struct {
    Storage* storage;
    File* file;
    uint16_t src_w;
    uint16_t src_h;
    bool bottom_up;
    uint16_t bpp;
    uint32_t data_offset;
    uint32_t stride;
    uint32_t palette[256]; /* 0x00RRGGBB */
    uint8_t mask_shift[3]; /* 32 bpp bitfields: r, g, b */
    uint32_t mask[3];
    bool bitfields;

    /* geometry */
    uint16_t out_w;
    uint16_t out_h;
    uint16_t oy;
    uint16_t dh;
    uint16_t crop_y;
    uint16_t crop_h;
    uint16_t* column_map; /* out x -> source x, or EZ_NO_COLUMN */

    uint8_t* raw_row;
    int32_t cached_row;
    uint16_t y;

    int16_t* err_a;
    int16_t* err_b;

    bool dither;
    bool invert;
    EzAccent accent;
} EzBmp;

static uint16_t rd16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}
static uint32_t rd32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint8_t ez_mask_shift(uint32_t mask) {
    uint8_t s = 0;
    if(!mask) return 0;
    while(!(mask & 1U)) {
        mask >>= 1;
        s++;
    }
    return s;
}

static uint8_t ez_mask_value(uint32_t px, uint32_t mask, uint8_t shift) {
    if(!mask) return 0;
    uint32_t v = (px & mask) >> shift;
    uint32_t max = mask >> shift;
    if(max == 255U) return (uint8_t)v;
    return (uint8_t)((v * 255U) / max);
}

typedef struct {
    uint16_t w;
    uint16_t h;
    bool bottom_up;
    uint16_t bpp;
    uint32_t compression;
    uint32_t data_offset;
    uint32_t header_size;
    uint32_t colors;
} EzBmpInfo;

static bool ez_bmp_read_info(File* file, EzBmpInfo* info, uint8_t* hdr, size_t hdr_cap) {
    if(!storage_file_seek(file, 0, true)) return false;
    size_t got = storage_file_read(file, hdr, hdr_cap);
    if(got < 54 || hdr[0] != 'B' || hdr[1] != 'M') return false;
    info->data_offset = rd32(&hdr[10]);
    info->header_size = rd32(&hdr[14]);
    if(info->header_size < 40) return false;
    int32_t w = (int32_t)rd32(&hdr[18]);
    int32_t h = (int32_t)rd32(&hdr[22]);
    if(w <= 0 || h == 0 || h == INT32_MIN) return false;
    info->bottom_up = h > 0;
    if(h < 0) h = -h;
    if((uint32_t)w > EZ_BMP_MAX_SIDE || (uint32_t)h > EZ_BMP_MAX_SIDE) return false;
    info->w = (uint16_t)w;
    info->h = (uint16_t)h;
    info->bpp = rd16(&hdr[28]);
    info->compression = rd32(&hdr[30]);
    info->colors = rd32(&hdr[46]);
    return true;
}

bool ez_bmp_probe(Storage* storage, const char* path, uint16_t* w, uint16_t* h, uint16_t* bpp) {
    File* file = storage_file_alloc(storage);
    bool ok = false;
    uint8_t hdr[64];
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        EzBmpInfo info;
        if(ez_bmp_read_info(file, &info, hdr, sizeof(hdr))) {
            if(w) *w = info.w;
            if(h) *h = info.h;
            if(bpp) *bpp = info.bpp;
            ok = true;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

static void ez_bmp_pixel(const EzBmp* bmp, uint16_t sx, uint8_t* r, uint8_t* g, uint8_t* b) {
    const uint8_t* row = bmp->raw_row;
    uint32_t c;
    switch(bmp->bpp) {
    case 1:
        c = bmp->palette[(row[sx >> 3] >> (7 - (sx & 7))) & 1U];
        break;
    case 4:
        c = bmp->palette[(sx & 1) ? (row[sx >> 1] & 0x0F) : (row[sx >> 1] >> 4)];
        break;
    case 8:
        c = bmp->palette[row[sx]];
        break;
    case 24: {
        const uint8_t* p = &row[sx * 3U];
        c = ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | p[0];
        break;
    }
    default: { /* 32 */
        uint32_t px = rd32(&row[sx * 4U]);
        if(bmp->bitfields) {
            c = ((uint32_t)ez_mask_value(px, bmp->mask[0], bmp->mask_shift[0]) << 16) |
                ((uint32_t)ez_mask_value(px, bmp->mask[1], bmp->mask_shift[1]) << 8) |
                ez_mask_value(px, bmp->mask[2], bmp->mask_shift[2]);
        } else {
            c = px & 0x00FFFFFFU;
        }
        break;
    }
    }
    *r = (uint8_t)(c >> 16);
    *g = (uint8_t)(c >> 8);
    *b = (uint8_t)c;
}

static bool ez_bmp_load_row(EzBmp* bmp, uint16_t sy) {
    if(bmp->cached_row == sy) return true;
    uint32_t file_row = bmp->bottom_up ? (uint32_t)(bmp->src_h - 1U - sy) : sy;
    uint32_t offset = bmp->data_offset + file_row * bmp->stride;
    if(!storage_file_seek(bmp->file, offset, true)) return false;
    if(storage_file_read(bmp->file, bmp->raw_row, bmp->stride) != bmp->stride) return false;
    bmp->cached_row = sy;
    return true;
}

static bool ez_bmp_restart(EzSource* src) {
    EzBmp* bmp = src->ctx;
    bmp->y = 0;
    bmp->cached_row = -1;
    memset(bmp->err_a, 0, sizeof(int16_t) * (bmp->out_w + 2U));
    memset(bmp->err_b, 0, sizeof(int16_t) * (bmp->out_w + 2U));
    return true;
}

static bool ez_bmp_read_row(EzSource* src, uint8_t* out) {
    EzBmp* bmp = src->ctx;
    const uint16_t w = bmp->out_w;
    const uint8_t paper = bmp->invert ? EZ_PX_BLACK : EZ_PX_WHITE;

    if(bmp->y >= bmp->out_h) return false;

    bool inside = bmp->y >= bmp->oy && bmp->y < (uint16_t)(bmp->oy + bmp->dh);
    int16_t* cur = bmp->err_a; /* errors for this row, index x+1 */
    int16_t* next = bmp->err_b;
    memset(next, 0, sizeof(int16_t) * (w + 2U));

    if(!inside) {
        memset(out, paper, w);
    } else {
        uint16_t sy =
            (uint16_t)(bmp->crop_y + ((uint32_t)(bmp->y - bmp->oy) * bmp->crop_h) / bmp->dh);
        if(sy >= bmp->src_h) sy = bmp->src_h - 1U;
        if(!ez_bmp_load_row(bmp, sy)) return false;

        for(uint16_t x = 0; x < w; x++) {
            uint16_t sx = bmp->column_map[x];
            if(sx == EZ_NO_COLUMN) {
                out[x] = paper;
                continue;
            }
            uint8_t r, g, b;
            ez_bmp_pixel(bmp, sx, &r, &g, &b);
            if(bmp->accent != EzAccentNone && ez_is_accent(r, g, b, bmp->accent)) {
                out[x] = EZ_PX_ACCENT;
                continue;
            }
            int16_t level = ez_luma(r, g, b);
            if(bmp->invert) level = (int16_t)(255 - level);
            if(bmp->dither) {
                int16_t want = (int16_t)(level + cur[x + 1]);
                bool white = want >= 128;
                int16_t err = (int16_t)(want - (white ? 255 : 0));
                out[x] = white ? EZ_PX_WHITE : EZ_PX_BLACK;
                /* Floyd-Steinberg: 7/16 right, 3/16 down-left, 5/16 down, 1/16 down-right */
                cur[x + 2] = (int16_t)(cur[x + 2] + (err * 7) / 16);
                next[x] = (int16_t)(next[x] + (err * 3) / 16);
                next[x + 1] = (int16_t)(next[x + 1] + (err * 5) / 16);
                next[x + 2] = (int16_t)(next[x + 2] + err / 16);
            } else {
                out[x] = (level >= 128) ? EZ_PX_WHITE : EZ_PX_BLACK;
            }
        }
    }

    bmp->err_a = next;
    bmp->err_b = cur;
    bmp->y++;
    return true;
}

static void ez_bmp_destroy(EzSource* src) {
    EzBmp* bmp = src->ctx;
    if(bmp) {
        if(bmp->file) {
            storage_file_close(bmp->file);
            storage_file_free(bmp->file);
        }
        free(bmp->column_map);
        free(bmp->raw_row);
        free(bmp->err_a);
        free(bmp->err_b);
        free(bmp);
    }
    free(src);
}

/* Maps the picture onto the output according to the fit mode. */
static void ez_bmp_layout(EzBmp* bmp, EzFit fit) {
    const uint32_t sw = bmp->src_w, sh = bmp->src_h;
    const uint32_t W = bmp->out_w, H = bmp->out_h;
    uint32_t ox = 0, oy = 0, dw = W, dh = H;
    uint32_t cx = 0, cy = 0, cw = sw, ch = sh;

    if(fit == EzFitContain) {
        if(sw * H <= sh * W) {
            dh = H;
            dw = (sw * H) / sh;
        } else {
            dw = W;
            dh = (sh * W) / sw;
        }
        if(dw < 1) dw = 1;
        if(dh < 1) dh = 1;
        ox = (W - dw) / 2U;
        oy = (H - dh) / 2U;
    } else if(fit == EzFitCover) {
        if(sw * H > sh * W) {
            cw = (sh * W) / H;
            if(cw < 1) cw = 1;
            cx = (sw - cw) / 2U;
        } else {
            ch = (sw * H) / W;
            if(ch < 1) ch = 1;
            cy = (sh - ch) / 2U;
        }
    }

    for(uint32_t x = 0; x < W; x++) {
        if(x < ox || x >= ox + dw) {
            bmp->column_map[x] = EZ_NO_COLUMN;
        } else {
            uint32_t sx = cx + ((x - ox) * cw) / dw;
            bmp->column_map[x] = (uint16_t)((sx < sw) ? sx : sw - 1U);
        }
    }
    bmp->oy = (uint16_t)oy;
    bmp->dh = (uint16_t)dh;
    bmp->crop_y = (uint16_t)cy;
    bmp->crop_h = (uint16_t)ch;
}

static EzSource* ez_bmp_create(
    const EzRenderSpec* spec,
    uint16_t width,
    uint16_t height,
    Storage* storage,
    const char** error) {
    EzSource* src = malloc(sizeof(EzSource));
    EzBmp* bmp = malloc(sizeof(EzBmp));
    memset(src, 0, sizeof(EzSource));
    memset(bmp, 0, sizeof(EzBmp));
    src->ctx = bmp;
    src->width = width;
    src->height = height;
    src->restart = ez_bmp_restart;
    src->read_row = ez_bmp_read_row;
    src->destroy = ez_bmp_destroy;

    const char* why = "Unreadable file";
    bool ok = false;
    uint8_t hdr[160];

    do {
        bmp->storage = storage;
        bmp->file = storage_file_alloc(storage);
        if(!storage_file_open(bmp->file, spec->path, FSAM_READ, FSOM_OPEN_EXISTING)) break;

        EzBmpInfo info;
        why = "Not a BMP image";
        if(!ez_bmp_read_info(bmp->file, &info, hdr, sizeof(hdr))) break;

        why = "BMP format not supported";
        if(!(info.bpp == 1 || info.bpp == 4 || info.bpp == 8 || info.bpp == 24 ||
             info.bpp == 32))
            break;
        if(info.compression == 3 && info.bpp == 32) {
            bmp->bitfields = true;
            for(uint8_t i = 0; i < 3; i++) {
                bmp->mask[i] = rd32(&hdr[54 + i * 4]);
                bmp->mask_shift[i] = ez_mask_shift(bmp->mask[i]);
            }
        } else if(info.compression != 0) {
            break;
        }

        bmp->src_w = info.w;
        bmp->src_h = info.h;
        bmp->bottom_up = info.bottom_up;
        bmp->bpp = info.bpp;
        bmp->data_offset = info.data_offset;
        bmp->stride = (((uint32_t)info.w * info.bpp + 31U) / 32U) * 4U;

        if(info.bpp <= 8) {
            uint32_t count = info.colors ? info.colors : (1U << info.bpp);
            if(count > 256U) count = 256U;
            uint32_t pal_offset = 14U + info.header_size;
            uint8_t entry[4];
            /* Default to a grey ramp in case the palette is short. */
            uint32_t full = 1U << info.bpp;
            for(uint32_t i = 0; i < full; i++) {
                uint8_t v = (uint8_t)((i * 255U) / (full - 1U));
                bmp->palette[i] = ((uint32_t)v << 16) | ((uint32_t)v << 8) | v;
            }
            if(!storage_file_seek(bmp->file, pal_offset, true)) break;
            for(uint32_t i = 0; i < count; i++) {
                if(storage_file_read(bmp->file, entry, 4) != 4) break;
                bmp->palette[i] = ((uint32_t)entry[2] << 16) | ((uint32_t)entry[1] << 8) |
                                  entry[0];
            }
        }

        why = "Out of memory";
        bmp->out_w = width;
        bmp->out_h = height;
        bmp->raw_row = malloc(bmp->stride);
        bmp->column_map = malloc(sizeof(uint16_t) * width);
        bmp->err_a = malloc(sizeof(int16_t) * (width + 2U));
        bmp->err_b = malloc(sizeof(int16_t) * (width + 2U));
        if(!bmp->raw_row || !bmp->column_map || !bmp->err_a || !bmp->err_b) break;

        bmp->dither = spec->dither;
        bmp->invert = spec->invert;
        bmp->accent = spec->accent;
        ez_bmp_layout(bmp, spec->fit);
        ez_bmp_restart(src);
        ok = true;
    } while(false);

    if(!ok) {
        if(error) *error = why;
        ez_bmp_destroy(src);
        return NULL;
    }
    return src;
}

/* ======================================================================== */
/* Text layout (used by the text source and the test card)                  */
/* ======================================================================== */

#define EZ_TEXT_LINES 8
#define EZ_CELL_W 6 /* glyph 5 + 1 spacing */
#define EZ_CELL_H 9 /* glyph 7 + 2 spacing */

typedef struct {
    char lines[EZ_TEXT_LINES][EZ_TEXT_MAX + 1];
    uint8_t line_len[EZ_TEXT_LINES];
    uint8_t count;
    uint8_t scale;
    uint16_t top; /* first text row, relative to the box */
    uint16_t box_x, box_y, box_w, box_h;
} EzTextLayout;

/* Splits text into words and wraps it to `cols` columns. */
static uint8_t ez_wrap(const char* text, uint8_t cols, EzTextLayout* lay, bool store) {
    uint8_t line = 0;
    uint8_t len = 0;
    const char* p = text;
    if(cols == 0) return 255;

    while(*p) {
        if(*p == '\n' || *p == '|') {
            line++;
            len = 0;
            p++;
            if(line >= EZ_TEXT_LINES) return 255;
            continue;
        }
        if(*p == ' ' || *p == '_') {
            if(len > 0 && len < cols) {
                if(store) lay->lines[line][len] = ' ';
                len++;
            }
            p++;
            continue;
        }
        /* measure the word */
        uint8_t wlen = 0;
        while(p[wlen] && p[wlen] != ' ' && p[wlen] != '_' && p[wlen] != '\n' && p[wlen] != '|')
            wlen++;
        if(len > 0 && len + wlen > cols) {
            /* drop a trailing space and wrap */
            if(store && len > 0 && lay->lines[line][len - 1] == ' ') len--;
            if(store) lay->line_len[line] = len;
            line++;
            len = 0;
            if(line >= EZ_TEXT_LINES) return 255;
        }
        for(uint8_t i = 0; i < wlen; i++) {
            if(len >= cols) {
                if(store) lay->line_len[line] = len;
                line++;
                len = 0;
                if(line >= EZ_TEXT_LINES) return 255;
            }
            if(store) lay->lines[line][len] = p[i];
            len++;
        }
        p += wlen;
        if(store) lay->line_len[line] = len;
    }
    if(store) {
        if(len > 0 && lay->lines[line][len - 1] == ' ') len--;
        lay->line_len[line] = len;
    }
    return (uint8_t)(line + 1);
}

static void ez_text_layout(
    EzTextLayout* lay,
    const char* text,
    uint16_t bx,
    uint16_t by,
    uint16_t bw,
    uint16_t bh) {
    memset(lay, 0, sizeof(*lay));
    lay->box_x = bx;
    lay->box_y = by;
    lay->box_w = bw;
    lay->box_h = bh;

    uint16_t max_scale = bh / EZ_CELL_H;
    if(bw / EZ_CELL_W < max_scale) max_scale = bw / EZ_CELL_W;
    if(max_scale < 1) max_scale = 1;
    if(max_scale > 40) max_scale = 40;

    uint8_t scale = 1;
    for(uint16_t s = max_scale; s >= 1; s--) {
        uint8_t cols = (uint8_t)((bw / (EZ_CELL_W * s)) > EZ_TEXT_MAX ? EZ_TEXT_MAX :
                                                                       bw / (EZ_CELL_W * s));
        uint8_t rows = (uint8_t)(bh / (EZ_CELL_H * s));
        uint8_t needed = ez_wrap(text, cols, lay, false);
        if(needed != 255 && needed <= rows) {
            scale = (uint8_t)s;
            break;
        }
    }
    uint8_t cols = (uint8_t)((bw / (EZ_CELL_W * scale)) > EZ_TEXT_MAX ? EZ_TEXT_MAX :
                                                                       bw / (EZ_CELL_W * scale));
    lay->scale = scale;
    uint8_t n = ez_wrap(text, cols, lay, true);
    if(n == 255) n = EZ_TEXT_LINES;
    lay->count = n;
    /* glyph block height: last line has no trailing spacing */
    uint16_t block = (uint16_t)(n * EZ_CELL_H * scale - 2U * scale);
    lay->top = (block < bh) ? (uint16_t)((bh - block) / 2U) : 0;
}

/* Paints the glyph pixels of row y into `out` using `ink`. */
static void ez_text_paint_row(const EzTextLayout* lay, uint16_t y, uint8_t* out, uint8_t ink) {
    if(y < lay->box_y + lay->top) return;
    uint16_t rel = (uint16_t)(y - lay->box_y - lay->top);
    uint16_t line_h = (uint16_t)(EZ_CELL_H * lay->scale);
    uint16_t line = rel / line_h;
    if(line >= lay->count) return;
    uint8_t glyph_row = (uint8_t)((rel % line_h) / lay->scale);
    if(glyph_row >= EZ_FONT_H) return;

    uint8_t len = lay->line_len[line];
    if(len == 0) return;
    uint16_t text_w = (uint16_t)(len * EZ_CELL_W * lay->scale - lay->scale);
    uint16_t x0 = (uint16_t)(lay->box_x + ((text_w < lay->box_w) ? (lay->box_w - text_w) / 2U : 0));

    for(uint8_t i = 0; i < len; i++) {
        char c = lay->lines[line][i];
        if(c == ' ') continue;
        uint16_t cx = (uint16_t)(x0 + i * EZ_CELL_W * lay->scale);
        for(uint8_t col = 0; col < EZ_FONT_W; col++) {
            if(!ez_font_pixel(c, col, glyph_row)) continue;
            uint16_t px = (uint16_t)(cx + col * lay->scale);
            for(uint8_t k = 0; k < lay->scale; k++) {
                if(px + k < lay->box_x + lay->box_w) out[px + k] = ink;
            }
        }
    }
}

/* ======================================================================== */
/* Text source and test card                                                */
/* ======================================================================== */

typedef struct {
    EzSourceKind kind;
    EzTextLayout lay;
    uint16_t y;
    uint16_t w, h;
    uint8_t paper;
    uint8_t ink;
    uint8_t frame; /* border thickness, 0 = none */
    EzAccent accent;
} EzProc;

static bool ez_proc_restart(EzSource* src) {
    ((EzProc*)src->ctx)->y = 0;
    return true;
}

static bool ez_proc_read_row(EzSource* src, uint8_t* out) {
    EzProc* p = src->ctx;
    if(p->y >= p->h) return false;
    const uint16_t y = p->y;
    const uint16_t w = p->w;
    memset(out, p->paper, w);

    uint8_t border_ink = (p->paper == EZ_PX_WHITE) ? EZ_PX_BLACK : EZ_PX_WHITE;
    if(p->frame) {
        if(y < p->frame || y >= p->h - p->frame) {
            memset(out, border_ink, w);
        } else {
            memset(out, border_ink, p->frame);
            memset(out + w - p->frame, border_ink, p->frame);
        }
    }

    if(p->kind == EzSourceTestCard) {
        /* top band: checkerboard, middle: caption, bottom: accent bar or ramp */
        uint16_t band = p->h / 6U;
        if(band < 4) band = 4;
        uint16_t inner_l = (uint16_t)(p->frame + 2U), inner_r = (uint16_t)(w - p->frame - 2U);
        uint16_t sq = band / 2U ? band / 2U : 1U;
        if(y >= p->frame + 2U && y < p->frame + 2U + band) {
            uint16_t row = (uint16_t)((y - p->frame - 2U) / sq);
            for(uint16_t x = inner_l; x < inner_r; x++) {
                bool dark = (((x - inner_l) / sq) + row) & 1U;
                out[x] = dark ? EZ_PX_BLACK : EZ_PX_WHITE;
            }
        } else if(y >= p->h - p->frame - 2U - band && y < p->h - p->frame - 2U) {
            if(p->accent != EzAccentNone) {
                for(uint16_t x = inner_l; x < inner_r; x++) {
                    uint16_t third = (uint16_t)((inner_r - inner_l) / 3U);
                    uint16_t rel = (uint16_t)(x - inner_l);
                    out[x] = (rel < third)      ? EZ_PX_BLACK :
                             (rel < 2U * third) ? EZ_PX_ACCENT :
                                                  EZ_PX_WHITE;
                }
            } else {
                /* ordered-dither grey ramp, black to white */
                static const uint8_t bayer[4][4] = {
                    {0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
                for(uint16_t x = inner_l; x < inner_r; x++) {
                    uint16_t level = (uint16_t)(((x - inner_l) * 16U) / (inner_r - inner_l));
                    out[x] = (level > bayer[y & 3][x & 3]) ? EZ_PX_WHITE : EZ_PX_BLACK;
                }
            }
        }
    }

    ez_text_paint_row(&p->lay, y, out, p->ink);
    p->y++;
    return true;
}

static void ez_proc_destroy(EzSource* src) {
    free(src->ctx);
    free(src);
}

static EzSource* ez_proc_create(const EzRenderSpec* spec, uint16_t width, uint16_t height) {
    EzSource* src = malloc(sizeof(EzSource));
    EzProc* p = malloc(sizeof(EzProc));
    memset(src, 0, sizeof(EzSource));
    memset(p, 0, sizeof(EzProc));
    src->ctx = p;
    src->width = width;
    src->height = height;
    src->restart = ez_proc_restart;
    src->read_row = ez_proc_read_row;
    src->destroy = ez_proc_destroy;

    p->kind = spec->kind;
    p->w = width;
    p->h = height;
    p->accent = spec->accent;
    p->paper = spec->invert ? EZ_PX_BLACK : EZ_PX_WHITE;
    p->ink = spec->invert ? EZ_PX_WHITE : EZ_PX_BLACK;
    if(spec->accent_text && spec->accent != EzAccentNone && !spec->invert) p->ink = EZ_PX_ACCENT;

    uint16_t short_side = (width < height) ? width : height;
    uint16_t margin;
    if(spec->kind == EzSourceTestCard) {
        p->frame = (uint8_t)((short_side >= 100) ? 3 : 2);
        uint16_t band = height / 6U;
        if(band < 4) band = 4;
        margin = (uint16_t)(p->frame + 4U);
        uint16_t top = (uint16_t)(p->frame + 2U + band + 2U);
        uint16_t bottom = (uint16_t)(p->frame + 2U + band + 2U);
        uint16_t box_h = (height > top + bottom) ? (uint16_t)(height - top - bottom) : height;
        ez_text_layout(
            &p->lay, spec->text, margin, top, (uint16_t)(width - 2U * margin), box_h);
    } else {
        p->frame = spec->frame ? (uint8_t)(short_side >= 100 ? 3 : 2) : 0;
        margin = (uint16_t)(p->frame + (short_side >= 100 ? 6U : 3U));
        if(margin * 2U >= width || margin * 2U >= height) margin = 0;
        ez_text_layout(
            &p->lay,
            spec->text,
            margin,
            margin,
            (uint16_t)(width - 2U * margin),
            (uint16_t)(height - 2U * margin));
    }
    return src;
}

/* ======================================================================== */
/* Rotation wrapper                                                         */
/* ======================================================================== */

typedef struct {
    EzSource* inner;
    uint8_t* pixels; /* 2 bits per inner pixel */
    bool rendered;
    uint16_t y;
} EzRot;

static inline void ez_rot_set(uint8_t* buf, uint32_t idx, uint8_t v) {
    uint32_t byte = idx >> 2;
    uint8_t shift = (uint8_t)((idx & 3U) * 2U);
    buf[byte] = (uint8_t)((buf[byte] & ~(3U << shift)) | ((v & 3U) << shift));
}
static inline uint8_t ez_rot_get(const uint8_t* buf, uint32_t idx) {
    return (buf[idx >> 2] >> ((idx & 3U) * 2U)) & 3U;
}

static bool ez_rot_restart(EzSource* src) {
    EzRot* r = src->ctx;
    r->y = 0;
    if(r->rendered) return true;
    EzSource* in = r->inner;
    uint8_t* row = malloc(in->width);
    if(!row) return false;
    bool ok = ez_source_restart(in);
    for(uint16_t y = 0; ok && y < in->height; y++) {
        ok = ez_source_row(in, row);
        for(uint16_t x = 0; ok && x < in->width; x++) {
            ez_rot_set(r->pixels, (uint32_t)y * in->width + x, row[x]);
        }
    }
    free(row);
    r->rendered = ok;
    return ok;
}

/* Output (px, py) on a (inner.h x inner.w) grid comes from inner
 * (x = py, y = inner.h - 1 - px). */
static bool ez_rot_read_row(EzSource* src, uint8_t* out) {
    EzRot* r = src->ctx;
    if(!r->rendered || r->y >= src->height) return false;
    const EzSource* in = r->inner;
    for(uint16_t px = 0; px < src->width; px++) {
        uint32_t iy = (uint32_t)(in->height - 1U - px);
        out[px] = ez_rot_get(r->pixels, iy * in->width + r->y);
    }
    r->y++;
    return true;
}

static void ez_rot_destroy(EzSource* src) {
    EzRot* r = src->ctx;
    if(r) {
        ez_source_free(r->inner);
        free(r->pixels);
        free(r);
    }
    free(src);
}

EzSource* ez_source_rotate(EzSource* inner) {
    if(!inner) return NULL;
    size_t bytes = ((size_t)inner->width * inner->height + 3U) / 4U;
    uint8_t* pixels = malloc(bytes);
    if(!pixels) {
        ez_source_free(inner);
        return NULL;
    }
    EzSource* src = malloc(sizeof(EzSource));
    EzRot* r = malloc(sizeof(EzRot));
    memset(src, 0, sizeof(EzSource));
    memset(r, 0, sizeof(EzRot));
    r->inner = inner;
    r->pixels = pixels;
    src->ctx = r;
    src->width = inner->height;
    src->height = inner->width;
    src->restart = ez_rot_restart;
    src->read_row = ez_rot_read_row;
    src->destroy = ez_rot_destroy;
    return src;
}

/* ======================================================================== */

EzSource* ez_source_create(
    const EzRenderSpec* spec,
    uint16_t width,
    uint16_t height,
    Storage* storage,
    const char** error) {
    if(!spec || width == 0 || height == 0) {
        if(error) *error = "Invalid size";
        return NULL;
    }
    if(spec->kind == EzSourceBmp) return ez_bmp_create(spec, width, height, storage, error);
    return ez_proc_create(spec, width, height);
}
