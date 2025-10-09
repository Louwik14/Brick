/**
 * @file drv_encoders.c
 * @brief Driver encodeurs rotatifs (TIM + quadrature soft) pour Brick.
 *
 * Gère jusqu’à 4 encodeurs rotatifs :
 * - ENC1 → TIM8 (hardware quadrature)
 * - ENC2 → TIM4
 * - ENC3 → quadrature logicielle sur GPIO
 * - ENC4 → TIM2
 *
 * Fournit :
 * - Lecture brute et delta normalisé
 * - Accélération dynamique (EMA + flick)
 *
 * @ingroup drivers
 */

#include "ch.h"
#include "hal.h"
#include "board.h"
#include "drv_encoders.h"
#include <math.h>

/* fallback si pas défini dans encoders.h */
#ifndef ENC_TICKS_PER_STEP
#define ENC_TICKS_PER_STEP 8
#endif
/* =======================================================================
 * Accélération EMA + Flick
 * ======================================================================= */

/* ------------ Variables internes ------------ */
static volatile int16_t enc3_count = 0;
static uint8_t enc3_last = 0;

/* Cache brut */
static int16_t last_values[NUM_ENCODERS] = {0};

typedef struct {
    int16_t   last_value;
    systime_t last_time;
} encoder_state_t;

static encoder_state_t states[NUM_ENCODERS];

typedef struct {
    float vel_ema;
    float impulse;
} enc_accel_state_t;

static enc_accel_state_t g_accel[NUM_ENCODERS];

/* ------------ Helpers ------------ */
static inline int16_t encoder_get_x1_hw(volatile uint32_t *cnt) {
    return (int16_t)(*cnt);
}
static inline int16_t encoder3_get_x1(void) {
    return enc3_count;
}

/* ------------ Quadrature soft ENC3 ------------ */
static void encoder3_update_irq(void) {
    uint8_t a = palReadLine(LINE_ENC3_A);
    uint8_t b = palReadLine(LINE_ENC3_B);
    uint8_t state = (a << 1) | b;

    static const int8_t table[16] = {
         0, -1, +1,  0,
        +1,  0,  0, -1,
        -1,  0,  0, +1,
         0, +1, -1,  0
    };

    uint8_t idx = (enc3_last << 2) | state;
    enc3_count += table[idx];
    enc3_last = state;
}

static void enc3_pal_cb(void *arg) {
    (void)arg;
    encoder3_update_irq();
}

/* ------------ Init hardware ------------ */
static void encoders_hw_init(void) {
    /* ENC1 : TIM8 */
    rccEnableTIM8(true);
    TIM8->SMCR  = TIM_SMCR_SMS_0 | TIM_SMCR_SMS_1;
    TIM8->CCMR1 = (TIM_CCMR1_CC1S_0) | (TIM_CCMR1_CC2S_0);
    TIM8->CCER  = 0;
    TIM8->ARR   = 0xFFFF;
    TIM8->CNT   = 0;
    TIM8->CR1   = TIM_CR1_CEN;

    /* ENC2 : TIM4 */
    rccEnableTIM4(true);
    TIM4->SMCR  = TIM_SMCR_SMS_0 | TIM_SMCR_SMS_1;
    TIM4->CCMR1 = (TIM_CCMR1_CC1S_0) | (TIM_CCMR1_CC2S_0);
    TIM4->CCER  = 0;
    TIM4->ARR   = 0xFFFF;
    TIM4->CNT   = 0;
    TIM4->CR1   = TIM_CR1_CEN;

    /* ENC4 : TIM2 */
    rccEnableTIM2(true);
    TIM2->SMCR  = TIM_SMCR_SMS_0 | TIM_SMCR_SMS_1;
    TIM2->CCMR1 = (TIM_CCMR1_CC1S_0) | (TIM_CCMR1_CC2S_0);
    TIM2->CCER  = 0;
    TIM2->ARR   = 0xFFFF;
    TIM2->CNT   = 0;
    TIM2->CR1   = TIM_CR1_CEN;

    /* ENC3 init soft */
    uint8_t a = palReadLine(LINE_ENC3_A);
    uint8_t b = palReadLine(LINE_ENC3_B);
    enc3_last = (a << 1) | b;
}

/* ------------ API publique ------------ */
void drv_encoders_start(void) {
    encoders_hw_init();

    palSetLineCallback(LINE_ENC3_A, enc3_pal_cb, NULL);
    palSetLineCallback(LINE_ENC3_B, enc3_pal_cb, NULL);
    palEnableLineEvent(LINE_ENC3_A, PAL_EVENT_MODE_BOTH_EDGES);
    palEnableLineEvent(LINE_ENC3_B, PAL_EVENT_MODE_BOTH_EDGES);

    systime_t now = chVTGetSystemTime();
    for (int i = 0; i < NUM_ENCODERS; i++) {
        int16_t val = drv_encoder_get((encoder_id_t)i);
        last_values[i]      = val;
        states[i].last_value= val;
        states[i].last_time = now;
        g_accel[i].vel_ema  = 0.0f;
        g_accel[i].impulse  = 0.0f;
    }
}

int16_t drv_encoder_get(encoder_id_t id) {
    switch (id) {
    case ENC1: return encoder_get_x1_hw(&TIM8->CNT);
    case ENC2: return encoder_get_x1_hw(&TIM4->CNT);
    case ENC3: return encoder3_get_x1();
    case ENC4: return encoder_get_x1_hw(&TIM2->CNT);
    default:   return 0;
    }
}

void encoder_reset(encoder_id_t id) {
    switch (id) {
    case ENC1: TIM8->CNT = 0; break;
    case ENC2: TIM4->CNT = 0; break;
    case ENC3: enc3_count = 0; break;
    case ENC4: TIM2->CNT = 0; break;
    default: break;
    }
    last_values[id]         = 0;
    states[id].last_value   = 0;
    states[id].last_time    = chVTGetSystemTime();
    g_accel[id].vel_ema     = 0.0f;
    g_accel[id].impulse     = 0.0f;
}

int16_t drv_encoder_get_delta(encoder_id_t id) {
    int16_t current = drv_encoder_get(id);
    int16_t delta   = current - last_values[id];
    last_values[id] = current;

    /* Normaliser en pas utilisateur */
    int steps = delta / ENC_TICKS_PER_STEP;
    if (steps == 0 && delta != 0) {
        steps = (delta > 0) ? 1 : -1;
    }
    return steps;
}

/* ------------ Accélération EMA + Flick ------------ */
int16_t drv_encoder_get_delta_accel(encoder_id_t id) {
    int16_t current = drv_encoder_get(id);
    int16_t delta_raw = current - states[id].last_value;

    if (delta_raw == 0) return 0;

    systime_t now   = chVTGetSystemTime();
    uint32_t  dt_ms = TIME_I2MS(now - states[id].last_time);
    if (dt_ms == 0) dt_ms = 1;
    if (dt_ms > 200) dt_ms = 200;

#if defined(ENC_DEBOUNCE_MS)
    if (dt_ms < ENC_DEBOUNCE_MS) return 0;
#endif

    states[id].last_value = current;
    states[id].last_time  = now;

    int   mag  = (delta_raw < 0) ? -delta_raw : delta_raw;
    int   sign = (delta_raw < 0) ? -1 : 1;
    float inst = (float)mag * (1000.0f / (float)dt_ms);

    float alpha = (float)dt_ms / (ENC_ACCEL_TAU_MS + (float)dt_ms);
    g_accel[id].vel_ema += alpha * (inst - g_accel[id].vel_ema);

    if (inst >= ENC_FLICK_THRESH) {
        g_accel[id].impulse += (inst - ENC_FLICK_THRESH) * ENC_FLICK_GAIN;
    } else {
        float dec = (float)dt_ms / ENC_FLICK_TAU_MS;
        if (dec > 1.0f) dec = 1.0f;
        g_accel[id].impulse -= g_accel[id].impulse * dec;
        if (g_accel[id].impulse < 0.0f) g_accel[id].impulse = 0.0f;
    }

    float v    = g_accel[id].vel_ema;
    float mult = 1.0f;
    if (v > ENC_ACCEL_V0) {
        if (v < ENC_ACCEL_V1) {
            mult = 1.0f + (v - ENC_ACCEL_V0) * ENC_ACCEL_G1;
        } else {
            float base = 1.0f + (ENC_ACCEL_V1 - ENC_ACCEL_V0) * ENC_ACCEL_G1;
            mult = base + (v - ENC_ACCEL_V1) * ENC_ACCEL_G2;
        }
    }

    mult += g_accel[id].impulse;
    if (mult > ENC_ACCEL_MAX) mult = ENC_ACCEL_MAX;

    int out_mag = (int)((float)mag * mult + 0.5f);
    if (out_mag == 0 && mag > 0) out_mag = 1;
    int out_ticks = sign * out_mag;

    /* → Normalisation en pas utilisateur */
    int steps = out_ticks / ENC_TICKS_PER_STEP;
    if (steps == 0 && out_ticks != 0) {
        steps = (out_ticks > 0) ? 1 : -1;
    }

    if (steps >  32767) steps =  32767;
    if (steps < -32768) steps = -32768;
    return (int16_t)steps;
}
