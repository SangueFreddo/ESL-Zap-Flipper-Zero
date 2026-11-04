/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 *
 * Infrared physical layer.
 *
 * Labels listen to short bursts of a ~1.25 MHz carrier. Information is
 * carried by the time between bursts (pulse position modulation):
 *   PP4  - 2 bits per burst, 4 possible gaps (all labels)
 *   PP16 - 4 bits per burst, 16 possible gaps (newer labels, ~2.5x faster)
 *
 * The stock infrared HAL tops out at 56 kHz, so ESL Zap drives the IR
 * timer directly and times symbols with the CPU cycle counter.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    EzPhyPP4 = 0,
    EzPhyPP16,
} EzPhy;

typedef enum {
    EzIrOutputInternal = 0, /* built-in IR LED */
    EzIrOutputExternal, /* external LED / booster on GPIO pin A7 */
} EzIrOutput;

/* Claims the IR hardware. False when the IR port is busy. */
bool ez_ir_open(EzIrOutput output);

/* Releases the IR hardware and parks the pin. Safe to call twice. */
void ez_ir_close(void);

/* Transmits one frame once. Interrupts are masked for the frame duration
 * (a few ms) to keep symbol timing exact. */
void ez_ir_send(const uint8_t* data, size_t len, EzPhy phy);

/* Air time of one frame in microseconds (for progress estimation). */
uint32_t ez_ir_frame_us(const uint8_t* data, size_t len, EzPhy phy);
