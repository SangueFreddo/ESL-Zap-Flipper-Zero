/*
 * ESL Zap host tests: renders sources and dumps protocol frames so they can
 * be compared with the reference implementation and inspected visually.
 * Usage:
 *   host_test address BARCODE
 *   host_test frames BARCODE file.bmp W H accent(0/1/2) page   -> hex frames
 *   host_test render kind(text|card|bmp) arg W H accent out.pgm
 */
#include <string.h>
#include "../../src/proto/ez_proto.h"
#include "../../src/proto/ez_models.h"
#include "../../src/image/ez_source.h"
#include "../../src/image/ez_encoder.h"
#include "../../src/proto/ez_nfc.h"

static void dump(const EzFrame* f) {
    for(int i = 0; i < f->len; i++) printf("%02X ", f->data[i]);
    printf("\n");
}

int main(int argc, char** argv) {
    if(argc >= 3 && !strcmp(argv[1], "address")) {
        uint8_t a[4];
        if(!ez_barcode_address(argv[2], a)) { printf("invalid\n"); return 1; }
        printf("%02X %02X %02X %02X model=%u\n", a[0], a[1], a[2], a[3], ez_barcode_model(argv[2]));
        return 0;
    }
    if(argc >= 3 && !strcmp(argv[1], "nfc")) {
        FILE* f = fopen(argv[2], "rb");
        if(!f) return 1;
        static char text[65536];
        size_t n = fread(text, 1, sizeof(text) - 1, f);
        text[n] = 0;
        fclose(f);
        char bc[EZ_BARCODE_LEN + 1] = {0}, uri[160] = {0};
        EzNfcResult r = ez_nfc_parse(text, bc, uri, sizeof(uri));
        printf("result=%d barcode=%s uri=%s\n", r, r == EzNfcOk ? bc : "-", uri);
        return 0;
    }
    if(argc >= 3 && !strcmp(argv[1], "nfcid")) {
        char bc[EZ_BARCODE_LEN + 1] = {0};
        bool bad = false;
        bool ok = ez_nfc_decode_id(argv[2], bc, &bad);
        printf("%s\n", ok ? bc : (bad ? "BADCHECK" : "INVALID"));
        return 0;
    }
    if(argc >= 8 && !strcmp(argv[1], "frames")) {
        uint8_t addr[4];
        ez_barcode_address(argv[2], addr);
        EzRenderSpec spec = {0};
        spec.kind = EzSourceBmp;
        strncpy(spec.path, argv[3], sizeof(spec.path) - 1);
        spec.fit = EzFitStretch;
        spec.dither = false;
        EzAccent accent = (EzAccent)atoi(argv[6]);
        spec.accent = accent;
        uint16_t w = atoi(argv[4]), h = atoi(argv[5]);
        const char* err = NULL;
        EzSource* src = ez_source_create(&spec, w, h, NULL, &err);
        if(!src) { printf("source error %s\n", err); return 1; }
        EzEncoder enc;
        ez_encoder_init(&enc, src, accent, true, 0, h);
        uint32_t rle = ez_encoder_measure(&enc);
        uint32_t raw = (uint32_t)w * h * (accent ? 2 : 1);
        bool use_rle = rle < raw;
        ez_encoder_deinit(&enc);
        ez_encoder_init(&enc, src, accent, use_rle, 0, h);
        uint32_t bits = use_rle ? rle : raw;
        uint32_t bytes = ez_encoder_padded_bytes(bits);
        EzFrame f;
        ez_frame_wake(&f, addr); dump(&f);
        EzImageHeader hd = {.byte_count = bytes, .compression = use_rle ? EzCompressionRle : EzCompressionRaw,
                            .page = atoi(argv[7]), .width = w, .height = h};
        ez_frame_image_header(&f, addr, &hd); dump(&f);
        uint8_t chunk[EZ_CHUNK_BYTES];
        for(uint32_t i = 0; i < bytes / EZ_CHUNK_BYTES; i++) {
            ez_encoder_chunk(&enc, chunk);
            ez_frame_image_chunk(&f, addr, i, chunk); dump(&f);
        }
        ez_frame_refresh(&f, addr); dump(&f);
        ez_encoder_deinit(&enc);
        ez_source_free(src);
        return 0;
    }
    if(argc >= 8 && !strcmp(argv[1], "render")) {
        EzRenderSpec spec = {0};
        spec.kind = !strcmp(argv[2], "text") ? EzSourceText : !strcmp(argv[2], "card") ? EzSourceTestCard : EzSourceBmp;
        if(spec.kind == EzSourceBmp) strncpy(spec.path, argv[3], 255); else strncpy(spec.text, argv[3], EZ_TEXT_MAX);
        uint16_t w = atoi(argv[4]), h = atoi(argv[5]);
        spec.accent = (EzAccent)atoi(argv[6]);
        spec.accent_text = spec.accent != 0;
        spec.dither = true;
        spec.frame = true;
        spec.fit = EzFitContain;
        bool rotate = argc >= 9 && !strcmp(argv[8], "rot");
        const char* err = NULL;
        EzSource* src = ez_source_create(&spec, w, h, NULL, &err);
        if(!src) { printf("error %s\n", err); return 1; }
        if(rotate) src = ez_source_rotate(src);
        ez_source_restart(src);
        FILE* o = fopen(argv[7], "wb");
        fprintf(o, "P6\n%u %u\n255\n", src->width, src->height);
        uint8_t* row = malloc(src->width);
        for(uint16_t y = 0; y < src->height; y++) {
            ez_source_row(src, row);
            for(uint16_t x = 0; x < src->width; x++) {
                uint8_t c[3] = {255, 255, 255};
                if(row[x] == EZ_PX_BLACK) c[0] = c[1] = c[2] = 0;
                if(row[x] == EZ_PX_ACCENT) { c[1] = spec.accent == EzAccentYellow ? 200 : 0; c[2] = 0; }
                fwrite(c, 1, 3, o);
            }
        }
        fclose(o);
        printf("ok %ux%u\n", src->width, src->height);
        ez_source_free(src);
        return 0;
    }
    printf("usage: see source\n");
    return 2;
}
