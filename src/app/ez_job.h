/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 *
 * A transmission job runs on a worker thread and reports progress
 * through a callback.
 */
#pragma once

#include "../image/ez_source.h"
#include "../ir/ez_ir.h"
#include "../proto/ez_models.h"

typedef enum {
    EzJobImage = 0,
    EzJobPage,
} EzJobKind;

typedef enum {
    EzPhasePrepare = 0,
    EzPhaseWake,
    EzPhaseData,
    EzPhaseFinish,
    EzPhaseDone,
    EzPhaseError,
    EzPhaseCancelled,
} EzPhase;

typedef enum {
    EzErrorNone = 0,
    EzErrorImage, /* see `detail` */
    EzErrorTooBig,
    EzErrorIrBusy,
    EzErrorMemory,
} EzError;

typedef struct {
    EzPhase phase;
    uint8_t percent; /* overall 0..100 */
    uint16_t strip; /* 1 based */
    uint16_t strips;
    uint32_t bytes; /* payload size of the image */
    bool compressed;
    EzError error;
    const char* detail;
} EzProgress;

typedef void (*EzProgressCallback)(const EzProgress* progress, void* context);

typedef struct {
    EzJobKind kind;
    uint8_t address[4];
    EzModel model;
    EzRenderSpec spec;
    uint8_t page;
    uint16_t page_seconds;

    EzPhy phy;
    EzIrOutput output;
    uint16_t wake_ms;
    uint8_t sends;

    Storage* storage;
    volatile bool* cancel;
    EzProgressCallback callback;
    void* context;
} EzJob;

/* FuriThread entry point; `context` is an EzJob*. */
int32_t ez_job_thread(void* context);

/* Wire size of the image for a model (rotated panels are sent sideways). */
void ez_job_wire_size(const EzModel* model, uint16_t* w, uint16_t* h);

/* Builds the source for a model, rotating it when needed. */
EzSource* ez_job_make_source(
    const EzRenderSpec* spec,
    const EzModel* model,
    Storage* storage,
    const char** error);
