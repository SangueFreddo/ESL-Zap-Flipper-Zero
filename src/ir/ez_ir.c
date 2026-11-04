/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 */
#include "ez_ir.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_bus.h>
#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>
#include <furi_hal_infrared.h>
#include <stm32wbxx_ll_tim.h>

/* Carrier: 64 MHz / 51 = 1.2549 MHz, ~50% duty (PWM2: low until compare). */
#define EZ_CARRIER_ARR 50U
#define EZ_CARRIER_CCR 25U

/* PP16 needs a fixed 4 byte preamble in front of every frame. */
static const uint8_t ez_pp16_preamble[4] = {0x00, 0x00, 0x00, 0x40};

/*
 * Symbol timing in microseconds, measured from the start of one burst to
 * the start of the next one. Index = symbol value.
 */
#define EZ_PP4_BURST_US 40U
static const uint16_t ez_pp4_period_us[4] = {101, 284, 162, 223};

#define EZ_PP16_BURST_US 20U
static const uint16_t ez_pp16_period_us[16] = {
    40, 64, 48, 56, 160, 136, 152, 144, 96, 72, 88, 80, 104, 128, 112, 120};

typedef struct {
    bool open;
    EzIrOutput output;
    volatile uint32_t* ccmr; /* CCMR register of the active channel */
    uint32_t ccmr_on;
    uint32_t ccmr_off;
    uint32_t cycles_per_us;
} EzIr;

static EzIr ez_ir = {0};

static const GpioPin* ez_ir_pin(EzIrOutput output) {
    return (output == EzIrOutputExternal) ? &gpio_ext_pa7 : &gpio_infrared_tx;
}

bool ez_ir_open(EzIrOutput output) {
    if(ez_ir.open) ez_ir_close();
    if(furi_hal_infrared_is_busy()) return false;

    if(furi_hal_bus_is_enabled(FuriHalBusTIM1)) furi_hal_bus_disable(FuriHalBusTIM1);
    furi_hal_bus_enable(FuriHalBusTIM1);

    uint32_t channel = (output == EzIrOutputExternal) ? LL_TIM_CHANNEL_CH1 : LL_TIM_CHANNEL_CH3;
    uint32_t channel_n = (output == EzIrOutputExternal) ? LL_TIM_CHANNEL_CH1N :
                                                          LL_TIM_CHANNEL_CH3N;

    LL_TIM_DisableCounter(TIM1);
    LL_TIM_SetPrescaler(TIM1, 0);
    LL_TIM_SetCounterMode(TIM1, LL_TIM_COUNTERMODE_UP);
    LL_TIM_SetAutoReload(TIM1, EZ_CARRIER_ARR);
    LL_TIM_SetRepetitionCounter(TIM1, 0);
    LL_TIM_SetCounter(TIM1, 0);
    LL_TIM_EnableARRPreload(TIM1);

    LL_TIM_OC_SetMode(TIM1, channel, LL_TIM_OCMODE_FORCED_INACTIVE);
    if(output == EzIrOutputExternal) {
        LL_TIM_OC_SetCompareCH1(TIM1, EZ_CARRIER_CCR);
    } else {
        LL_TIM_OC_SetCompareCH3(TIM1, EZ_CARRIER_CCR);
    }
    LL_TIM_OC_EnablePreload(TIM1, channel);
    LL_TIM_OC_DisableFast(TIM1, channel);
    LL_TIM_OC_SetPolarity(TIM1, channel_n, LL_TIM_OCPOLARITY_HIGH);
    LL_TIM_CC_EnableChannel(TIM1, channel_n);
    LL_TIM_DisableMasterSlaveMode(TIM1);
    LL_TIM_EnableAllOutputs(TIM1);
    LL_TIM_GenerateEvent_UPDATE(TIM1);
    LL_TIM_EnableCounter(TIM1);

    /* Pre-compute the two CCMR values so switching the carrier is one store. */
    if(output == EzIrOutputExternal) {
        ez_ir.ccmr = &TIM1->CCMR1;
        uint32_t base = TIM1->CCMR1 & ~TIM_CCMR1_OC1M;
        ez_ir.ccmr_off = base | TIM_CCMR1_OC1M_2; /* forced inactive */
        ez_ir.ccmr_on = base | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_0; /* PWM2 */
    } else {
        ez_ir.ccmr = &TIM1->CCMR2;
        uint32_t base = TIM1->CCMR2 & ~TIM_CCMR2_OC3M;
        ez_ir.ccmr_off = base | TIM_CCMR2_OC3M_2;
        ez_ir.ccmr_on = base | TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_0;
    }
    *ez_ir.ccmr = ez_ir.ccmr_off;

    const GpioPin* pin = ez_ir_pin(output);
    furi_hal_gpio_write(pin, false);
    furi_hal_gpio_init_ex(
        pin, GpioModeAltFunctionPushPull, GpioPullNo, GpioSpeedVeryHigh, GpioAltFn1TIM1);

    ez_ir.cycles_per_us = furi_hal_cortex_instructions_per_microsecond();
    if(ez_ir.cycles_per_us == 0) ez_ir.cycles_per_us = 64;
    ez_ir.output = output;
    ez_ir.open = true;
    return true;
}

void ez_ir_close(void) {
    if(!ez_ir.open) return;
    *ez_ir.ccmr = ez_ir.ccmr_off;
    LL_TIM_DisableAllOutputs(TIM1);
    LL_TIM_DisableCounter(TIM1);
    if(furi_hal_bus_is_enabled(FuriHalBusTIM1)) furi_hal_bus_disable(FuriHalBusTIM1);
    furi_hal_gpio_init(ez_ir_pin(ez_ir.output), GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    ez_ir.open = false;
}

static inline void ez_wait_until(uint32_t deadline) {
    while((int32_t)(DWT->CYCCNT - deadline) < 0) {
    }
}

typedef struct {
    uint32_t burst;
    uint32_t period[16];
    uint8_t bits; /* bits per symbol */
} EzPhyTiming;

static void ez_phy_timing(EzPhy phy, EzPhyTiming* t, uint32_t cpu) {
    if(phy == EzPhyPP16) {
        t->bits = 4;
        t->burst = EZ_PP16_BURST_US * cpu;
        for(uint8_t i = 0; i < 16; i++)
            t->period[i] = ez_pp16_period_us[i] * cpu;
    } else {
        t->bits = 2;
        t->burst = EZ_PP4_BURST_US * cpu;
        for(uint8_t i = 0; i < 4; i++)
            t->period[i] = ez_pp4_period_us[i] * cpu;
    }
}

/* Symbols are taken least significant first from every byte. */
static inline uint32_t
    ez_send_bytes(const uint8_t* data, size_t len, const EzPhyTiming* t, uint32_t start) {
    const uint8_t mask = (uint8_t)((1U << t->bits) - 1U);
    const uint8_t per_byte = (uint8_t)(8U / t->bits);
    volatile uint32_t* ccmr = ez_ir.ccmr;
    const uint32_t on = ez_ir.ccmr_on;
    const uint32_t off = ez_ir.ccmr_off;
    uint32_t mark = start;
    for(size_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        for(uint8_t s = 0; s < per_byte; s++) {
            uint8_t symbol = byte & mask;
            byte >>= t->bits;
            ez_wait_until(mark);
            *ccmr = on;
            ez_wait_until(mark + t->burst);
            *ccmr = off;
            mark += t->period[symbol];
        }
    }
    return mark;
}

void ez_ir_send(const uint8_t* data, size_t len, EzPhy phy) {
    if(!ez_ir.open || !data || !len) return;

    EzPhyTiming timing;
    ez_phy_timing(phy, &timing, ez_ir.cycles_per_us);

    FURI_CRITICAL_ENTER();
    uint32_t mark = DWT->CYCCNT + 64U;
    if(phy == EzPhyPP16) {
        mark = ez_send_bytes(ez_pp16_preamble, sizeof(ez_pp16_preamble), &timing, mark);
    }
    mark = ez_send_bytes(data, len, &timing, mark);
    /* Closing burst marks the end of the last symbol. */
    ez_wait_until(mark);
    *ez_ir.ccmr = ez_ir.ccmr_on;
    ez_wait_until(mark + timing.burst);
    *ez_ir.ccmr = ez_ir.ccmr_off;
    FURI_CRITICAL_EXIT();
}

uint32_t ez_ir_frame_us(const uint8_t* data, size_t len, EzPhy phy) {
    uint32_t total = 0;
    uint8_t bits = (phy == EzPhyPP16) ? 4 : 2;
    uint8_t mask = (uint8_t)((1U << bits) - 1U);
    const uint16_t* period = (phy == EzPhyPP16) ? ez_pp16_period_us : ez_pp4_period_us;
    if(phy == EzPhyPP16) total += 4U * 2U * 100U;
    for(size_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        for(uint8_t s = 0; s < 8 / bits; s++) {
            total += period[byte & mask];
            byte >>= bits;
        }
    }
    return total + 100U;
}
