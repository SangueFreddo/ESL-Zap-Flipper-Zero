/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 */
#include "ez_job.h"
#include "../image/ez_encoder.h"
#include "../proto/ez_proto.h"

#include <furi.h>
#include <string.h>

#define EZ_MAX_PAYLOAD 64000U /* header byte count is 16 bit */
#define EZ_MAX_STRIPS 16U
#define EZ_GAP_REPEAT_MS 3U /* between copies of the same frame */
#define EZ_GAP_FRAME_MS 2U /* between different frames */
#define EZ_STRIP_SETTLE_MS 1500U

void ez_job_wire_size(const EzModel* model, uint16_t* w, uint16_t* h) {
    if(model->flags & EZ_MODEL_ROTATED) {
        *w = model->height;
        *h = model->width;
    } else {
        *w = model->width;
        *h = model->height;
    }
}

EzSource* ez_job_make_source(
    const EzRenderSpec* spec,
    const EzModel* model,
    Storage* storage,
    const char** error) {
    EzSource* src = ez_source_create(spec, model->width, model->height, storage, error);
    if(src && (model->flags & EZ_MODEL_ROTATED)) {
        src = ez_source_rotate(src);
        if(!src && error) *error = "Out of memory";
    }
    return src;
}

typedef struct {
    EzJob* job;
    EzProgress progress;
    uint32_t total_units; /* progress units: wake ms + data frames */
    uint32_t done_units;
} EzRun;

static bool ez_cancelled(EzRun* run) {
    return run->job->cancel && *run->job->cancel;
}

static void ez_report(EzRun* run) {
    if(run->total_units) {
        uint32_t pct = (run->done_units * 100U) / run->total_units;
        run->progress.percent = (uint8_t)(pct > 100U ? 100U : pct);
    }
    if(run->job->callback) run->job->callback(&run->progress, run->job->context);
}

static void ez_phase(EzRun* run, EzPhase phase) {
    run->progress.phase = phase;
    ez_report(run);
}

/* Sends one frame `copies` times. */
static void ez_send(EzRun* run, const EzFrame* f, uint8_t copies) {
    for(uint8_t i = 0; i < copies; i++) {
        ez_ir_send(f->data, f->len, run->job->phy);
        furi_delay_ms(EZ_GAP_REPEAT_MS);
    }
}

/* Repeats a frame for `ms` milliseconds. Returns false when cancelled. */
static bool ez_repeat_for(EzRun* run, const EzFrame* f, uint32_t ms, uint32_t units) {
    uint32_t start = furi_get_tick();
    uint32_t span = furi_ms_to_ticks(ms);
    uint32_t last_report = 0;
    uint32_t base = run->done_units;
    while(true) {
        uint32_t elapsed = furi_get_tick() - start;
        if(elapsed >= span) break;
        if(ez_cancelled(run)) return false;
        ez_ir_send(f->data, f->len, run->job->phy);
        furi_delay_ms(EZ_GAP_REPEAT_MS);
        if(elapsed - last_report >= furi_ms_to_ticks(150)) {
            last_report = elapsed;
            run->done_units = base + (units * elapsed) / span;
            ez_report(run);
        }
    }
    run->done_units = base + units;
    return true;
}

typedef struct {
    uint16_t y0;
    uint16_t rows;
    uint32_t bits;
    bool rle;
} EzStrip;

/* Picks raw or RLE for a strip and measures it. */
static bool ez_plan_strip(EzSource* src, EzAccent accent, EzStrip* strip) {
    EzEncoder enc;
    if(!ez_encoder_init(&enc, src, accent, true, strip->y0, strip->rows)) {
        ez_encoder_deinit(&enc);
        return false;
    }
    uint32_t rle_bits = ez_encoder_measure(&enc);
    bool failed = enc.failed;
    ez_encoder_deinit(&enc);
    if(failed) return false;
    uint32_t raw_bits = (uint32_t)src->width * strip->rows * (accent != EzAccentNone ? 2U : 1U);
    strip->rle = rle_bits < raw_bits;
    strip->bits = strip->rle ? rle_bits : raw_bits;
    return true;
}

static int32_t ez_job_image(EzRun* run) {
    EzJob* job = run->job;
    const char* why = NULL;
    EzStrip strips[EZ_MAX_STRIPS];
    uint16_t strip_count = 1;
    int32_t result = -1;

    ez_phase(run, EzPhasePrepare);

    EzSource* src = ez_job_make_source(&job->spec, &job->model, job->storage, &why);
    if(!src) {
        run->progress.error = EzErrorImage;
        run->progress.detail = why ? why : "Image error";
        return -1;
    }

    /* Plan: one strip if it fits the 16 bit size field, otherwise split. */
    bool planned = false;
    for(strip_count = 1; strip_count <= EZ_MAX_STRIPS && !planned; strip_count++) {
        uint16_t base = src->height / strip_count;
        bool fits = true;
        uint32_t total = 0;
        for(uint16_t i = 0; i < strip_count && fits; i++) {
            strips[i].y0 = (uint16_t)(i * base);
            strips[i].rows =
                (i == strip_count - 1U) ? (uint16_t)(src->height - strips[i].y0) : base;
            if(!ez_plan_strip(src, job->model.accent, &strips[i])) {
                run->progress.error = EzErrorImage;
                run->progress.detail = "Read error";
                ez_source_free(src);
                return -1;
            }
            if(ez_encoder_padded_bytes(strips[i].bits) > EZ_MAX_PAYLOAD) fits = false;
            total += ez_encoder_padded_bytes(strips[i].bits);
            if(ez_cancelled(run)) {
                ez_source_free(src);
                return -2;
            }
        }
        if(fits) {
            planned = true;
            run->progress.bytes = total;
            run->progress.compressed = strips[0].rle;
            break;
        }
    }
    if(!planned) {
        run->progress.error = EzErrorTooBig;
        ez_source_free(src);
        return -1;
    }
    run->progress.strips = strip_count;

    /* Progress units: 1 unit per ms of wake + 40 units per data frame. */
    uint32_t frames_total = 0;
    for(uint16_t i = 0; i < strip_count; i++)
        frames_total += ez_encoder_padded_bytes(strips[i].bits) / EZ_CHUNK_BYTES;
    uint32_t frame_units = 10U * job->sends * (job->phy == EzPhyPP16 ? 1U : 3U);
    run->total_units = job->wake_ms + frames_total * frame_units +
                       (strip_count - 1U) * EZ_STRIP_SETTLE_MS;
    run->done_units = 0;

    if(!ez_ir_open(job->output)) {
        run->progress.error = EzErrorIrBusy;
        ez_source_free(src);
        return -1;
    }

    EzFrame frame;
    do {
        ez_phase(run, EzPhaseWake);
        ez_frame_wake(&frame, job->address);
        if(!ez_repeat_for(run, &frame, job->wake_ms, job->wake_ms)) {
            result = -2;
            break;
        }
        furi_delay_ms(50);

        bool aborted = false;
        for(uint16_t s = 0; s < strip_count && !aborted; s++) {
            run->progress.strip = (uint16_t)(s + 1U);
            ez_phase(run, EzPhaseData);

            EzEncoder enc;
            if(!ez_encoder_init(
                   &enc, src, job->model.accent, strips[s].rle, strips[s].y0, strips[s].rows)) {
                ez_encoder_deinit(&enc);
                run->progress.error = EzErrorMemory;
                aborted = true;
                break;
            }

            uint32_t bytes = ez_encoder_padded_bytes(strips[s].bits);
            EzImageHeader header = {
                .byte_count = (uint16_t)bytes,
                .compression = strips[s].rle ? EzCompressionRle : EzCompressionRaw,
                .page = job->page,
                .width = src->width,
                .height = strips[s].rows,
                .x = 0,
                .y = strips[s].y0,
            };
            ez_frame_image_header(&frame, job->address, &header);
            ez_send(run, &frame, job->sends);
            furi_delay_ms(20);

            uint8_t chunk[EZ_CHUNK_BYTES];
            uint16_t chunks = (uint16_t)(bytes / EZ_CHUNK_BYTES);
            for(uint16_t c = 0; c < chunks; c++) {
                if(ez_cancelled(run)) {
                    aborted = true;
                    result = -2;
                    break;
                }
                if(!ez_encoder_chunk(&enc, chunk)) {
                    run->progress.error = EzErrorImage;
                    run->progress.detail = "Read error";
                    aborted = true;
                    break;
                }
                ez_frame_image_chunk(&frame, job->address, c, chunk);
                ez_send(run, &frame, job->sends);
                furi_delay_ms(EZ_GAP_FRAME_MS);
                run->done_units += frame_units;
                if((c & 3U) == 3U || c + 1U == chunks) ez_report(run);
            }
            ez_encoder_deinit(&enc);
            if(aborted) break;

            ez_phase(run, EzPhaseFinish);
            ez_frame_refresh(&frame, job->address);
            ez_send(run, &frame, job->sends);

            if(s + 1U < strip_count) {
                furi_delay_ms(EZ_STRIP_SETTLE_MS);
                run->done_units += EZ_STRIP_SETTLE_MS;
            }
        }
        if(aborted) break;
        result = 0;
    } while(false);

    ez_ir_close();
    ez_source_free(src);
    return result;
}

static int32_t ez_job_page(EzRun* run) {
    EzJob* job = run->job;
    run->total_units = job->wake_ms;
    run->done_units = 0;
    if(!ez_ir_open(job->output)) {
        run->progress.error = EzErrorIrBusy;
        return -1;
    }
    EzFrame frame;
    if(job->model.kind == EzKindSegment) {
        ez_frame_show_page_segment(&frame, job->address, job->page, job->page_seconds);
    } else {
        ez_frame_show_page(&frame, job->address, job->page, job->page_seconds);
    }
    ez_phase(run, EzPhaseWake);
    /* The page command is repeated long enough to also wake the label. */
    bool ok = ez_repeat_for(run, &frame, job->wake_ms, job->wake_ms);
    ez_ir_close();
    return ok ? 0 : -2;
}

int32_t ez_job_thread(void* context) {
    EzJob* job = context;
    EzRun run;
    memset(&run, 0, sizeof(run));
    run.job = job;
    run.progress.strips = 1;

    int32_t result = (job->kind == EzJobPage) ? ez_job_page(&run) : ez_job_image(&run);

    if(result == 0) {
        run.done_units = run.total_units;
        ez_phase(&run, EzPhaseDone);
    } else if(result == -2 || ez_cancelled(&run)) {
        ez_phase(&run, EzPhaseCancelled);
    } else {
        ez_phase(&run, EzPhaseError);
    }
    return result;
}
