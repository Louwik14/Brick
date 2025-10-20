/**
 * @file drv_encoders.c
 * @brief Driver encodeurs (quadrature matériel/logiciel + step/dir) pour Brick.
 *
 * Implémente une lecture normalisée à 1 pas par cran avec accélération basée
 * sur EMA + seuils + hystérésis. L’ISR reste minimal : simple incrément des
 * compteurs, sans allocation ni attente.
 */

#include "ch.h"
#include "hal.h"
#include "board.h"
#include "drv_encoders.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

/* -------------------------------------------------------------------------
 *  Modes matériels supportés
 * ------------------------------------------------------------------------- */
typedef enum {
    ENC_MODE_HW_QUADRATURE,   /**< Timer matériel en mode quadrature. */
    ENC_MODE_SOFT_QUADRATURE, /**< Quadrature software via callbacks GPIO. */
    ENC_MODE_STEP_DIR         /**< Entrée step/dir sur ligne d’IRQ externe. */
} encoder_mode_t;

/* -------------------------------------------------------------------------
 *  Configuration compile-time (surchargable via macros)
 * ------------------------------------------------------------------------- */
#ifndef ENC_CFG_MODE_ENC3
#define ENC_CFG_MODE_ENC3 ENC_MODE_STEP_DIR
#endif

#ifndef ENC_CFG_TICKS_ENC1
#define ENC_CFG_TICKS_ENC1 4
#endif
#ifndef ENC_CFG_TICKS_ENC2
#define ENC_CFG_TICKS_ENC2 4
#endif
#ifndef ENC_CFG_TICKS_ENC3
 #if ENC_CFG_MODE_ENC3 == ENC_MODE_STEP_DIR
  #define ENC_CFG_TICKS_ENC3 1
 #else
  #define ENC_CFG_TICKS_ENC3 4
 #endif
#endif
#ifndef ENC_CFG_TICKS_ENC4
#define ENC_CFG_TICKS_ENC4 4
#endif

#ifndef ENC_CFG_POLARITY_ENC1
#define ENC_CFG_POLARITY_ENC1 (+1)
#endif
#ifndef ENC_CFG_POLARITY_ENC2
#define ENC_CFG_POLARITY_ENC2 (+1)
#endif
#ifndef ENC_CFG_POLARITY_ENC3
#define ENC_CFG_POLARITY_ENC3 (+1)
#endif
#ifndef ENC_CFG_POLARITY_ENC4
#define ENC_CFG_POLARITY_ENC4 (+1)
#endif

#ifndef ENC_ACCEL_TAU_MS
#define ENC_ACCEL_TAU_MS 90.0f
#endif
#ifndef ENC_ACCEL_LEVEL1_ON
#define ENC_ACCEL_LEVEL1_ON 35.0f
#endif
#ifndef ENC_ACCEL_LEVEL1_OFF
#define ENC_ACCEL_LEVEL1_OFF 25.0f
#endif
#ifndef ENC_ACCEL_LEVEL2_ON
#define ENC_ACCEL_LEVEL2_ON 95.0f
#endif
#ifndef ENC_ACCEL_LEVEL2_OFF
#define ENC_ACCEL_LEVEL2_OFF 70.0f
#endif
#ifndef ENC_ACCEL_LEVEL1_GAIN
#define ENC_ACCEL_LEVEL1_GAIN 2
#endif
#ifndef ENC_ACCEL_LEVEL2_GAIN
#define ENC_ACCEL_LEVEL2_GAIN 4
#endif
#ifndef ENC_ACCEL_MAX_GAIN
#define ENC_ACCEL_MAX_GAIN 8
#endif
#ifndef ENC_ACCEL_IDLE_RESET_MS
#define ENC_ACCEL_IDLE_RESET_MS 250U
#endif

#if (ENC_CFG_TICKS_ENC1 <= 0) || (ENC_CFG_TICKS_ENC2 <= 0) || \
    (ENC_CFG_TICKS_ENC3 <= 0) || (ENC_CFG_TICKS_ENC4 <= 0)
#error "ENC_CFG_TICKS_* doit être > 0"
#endif

#if ((ENC_CFG_POLARITY_ENC1 != 1) && (ENC_CFG_POLARITY_ENC1 != -1)) || \
    ((ENC_CFG_POLARITY_ENC2 != 1) && (ENC_CFG_POLARITY_ENC2 != -1)) || \
    ((ENC_CFG_POLARITY_ENC3 != 1) && (ENC_CFG_POLARITY_ENC3 != -1)) || \
    ((ENC_CFG_POLARITY_ENC4 != 1) && (ENC_CFG_POLARITY_ENC4 != -1))
#error "ENC_CFG_POLARITY_* doit valoir +1 ou -1"
#endif

#if (ENC_ACCEL_LEVEL1_ON < ENC_ACCEL_LEVEL1_OFF)
#error "ENC_ACCEL_LEVEL1_ON doit être >= ENC_ACCEL_LEVEL1_OFF"
#endif
#if (ENC_ACCEL_LEVEL2_ON < ENC_ACCEL_LEVEL2_OFF)
#error "ENC_ACCEL_LEVEL2_ON doit être >= ENC_ACCEL_LEVEL2_OFF"
#endif
#if (ENC_ACCEL_LEVEL2_ON < ENC_ACCEL_LEVEL1_ON)
#error "ENC_ACCEL_LEVEL2_ON doit être >= ENC_ACCEL_LEVEL1_ON"
#endif
#if (ENC_ACCEL_LEVEL1_GAIN < 1) || (ENC_ACCEL_LEVEL2_GAIN < 1)
#error "ENC_ACCEL_LEVEL*_GAIN doit être >= 1"
#endif
#if (ENC_ACCEL_MAX_GAIN < ENC_ACCEL_LEVEL2_GAIN)
#error "ENC_ACCEL_MAX_GAIN doit être >= ENC_ACCEL_LEVEL2_GAIN"
#endif
#if (ENC_ACCEL_IDLE_RESET_MS == 0U)
#error "ENC_ACCEL_IDLE_RESET_MS doit être > 0"
#endif

/* -------------------------------------------------------------------------
 *  Descripteurs encodeurs
 * ------------------------------------------------------------------------- */
typedef struct {
    encoder_mode_t mode;
    uint8_t        ticks_per_detent;
    int8_t         polarity;
} encoder_cfg_t;

static const encoder_cfg_t g_encoder_cfg[NUM_ENCODERS] = {
    [ENC1] = {ENC_MODE_HW_QUADRATURE, ENC_CFG_TICKS_ENC1, ENC_CFG_POLARITY_ENC1},
    [ENC2] = {ENC_MODE_HW_QUADRATURE, ENC_CFG_TICKS_ENC2, ENC_CFG_POLARITY_ENC2},
#if ENC_CFG_MODE_ENC3 == ENC_MODE_SOFT_QUADRATURE
    [ENC3] = {ENC_MODE_SOFT_QUADRATURE, ENC_CFG_TICKS_ENC3, ENC_CFG_POLARITY_ENC3},
#elif ENC_CFG_MODE_ENC3 == ENC_MODE_STEP_DIR
    [ENC3] = {ENC_MODE_STEP_DIR, ENC_CFG_TICKS_ENC3, ENC_CFG_POLARITY_ENC3},
#else
#error "Mode ENC3 non supporté"
#endif
    [ENC4] = {ENC_MODE_HW_QUADRATURE, ENC_CFG_TICKS_ENC4, ENC_CFG_POLARITY_ENC4},
};

/* -------------------------------------------------------------------------
 *  États runtime
 * ------------------------------------------------------------------------- */
typedef struct {
    int16_t   last_raw;
    systime_t last_time;
    float     ema_speed;
    uint8_t   accel_level;
    int8_t    last_sign;
} encoder_accel_state_t;

static int16_t              g_last_simple[NUM_ENCODERS];
static int16_t              g_residual_ticks[NUM_ENCODERS];
static encoder_accel_state_t g_accel_state[NUM_ENCODERS];

/* -------------------------------------------------------------------------
 *  Comptage spécifiques
 * ------------------------------------------------------------------------- */
#if ENC_CFG_MODE_ENC3 == ENC_MODE_SOFT_QUADRATURE
static volatile int16_t enc3_count = 0;
static uint8_t          enc3_last  = 0;
#elif ENC_CFG_MODE_ENC3 == ENC_MODE_STEP_DIR
static volatile int16_t enc3_step_count = 0;
#endif

/* -------------------------------------------------------------------------
 *  Helpers bas niveau
 * ------------------------------------------------------------------------- */
static inline bool encoder_id_valid(encoder_id_t id) {
    return (unsigned)id < (unsigned)NUM_ENCODERS;
}

static inline int16_t encoder_hw_get(volatile uint32_t *cnt) {
    return (int16_t)(*cnt);
}

static inline void encoder_hw_set(volatile uint32_t *cnt, int16_t value) {
    *cnt = (uint32_t)((uint16_t)value);
}

static inline int16_t saturate_to_int16(int32_t value) {
    if (value > INT16_MAX) {
        return INT16_MAX;
    }
    if (value < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t)value;
}

static inline uint8_t encoder_update_level(uint8_t level, float speed) {
    switch (level) {
    case 0:
        if (speed >= ENC_ACCEL_LEVEL2_ON) {
            return 2;
        }
        if (speed >= ENC_ACCEL_LEVEL1_ON) {
            return 1;
        }
        return 0;
    case 1:
        if (speed >= ENC_ACCEL_LEVEL2_ON) {
            return 2;
        }
        if (speed < ENC_ACCEL_LEVEL1_OFF) {
            return 0;
        }
        return 1;
    case 2:
        if (speed < ENC_ACCEL_LEVEL2_OFF) {
            if (speed < ENC_ACCEL_LEVEL1_OFF) {
                return 0;
            }
            return 1;
        }
        return 2;
    default:
        return 0;
    }
}

static int16_t encoder_normalize_delta(encoder_id_t id, int16_t raw_delta) {
    const encoder_cfg_t *cfg = &g_encoder_cfg[id];
    int32_t scaled = (int32_t)raw_delta * (int32_t)cfg->polarity + g_residual_ticks[id];

    if (scaled == 0) {
        g_residual_ticks[id] = 0;
        return 0;
    }

    int32_t ticks = (int32_t)cfg->ticks_per_detent;
    int32_t detents = scaled / ticks;
    int32_t remainder = scaled % ticks;

    if (detents == 0) {
        if (scaled > 0) {
            detents = 1;
            remainder = scaled - ticks;
        } else {
            detents = -1;
            remainder = scaled + ticks;
        }
    }

    g_residual_ticks[id] = (int16_t)remainder;
    return saturate_to_int16(detents);
}

static int16_t encoder_read_raw(encoder_id_t id) {
    switch (id) {
    case ENC1: return encoder_hw_get(&TIM8->CNT);
    case ENC2: return encoder_hw_get(&TIM4->CNT);
    case ENC4: return encoder_hw_get(&TIM2->CNT);
    case ENC3:
#if ENC_CFG_MODE_ENC3 == ENC_MODE_SOFT_QUADRATURE
        return enc3_count;
#elif ENC_CFG_MODE_ENC3 == ENC_MODE_STEP_DIR
        return enc3_step_count;
#endif
    default:
        return 0;
    }
}

static void encoder_write_raw(encoder_id_t id, int16_t value) {
    switch (id) {
    case ENC1: encoder_hw_set(&TIM8->CNT, value); break;
    case ENC2: encoder_hw_set(&TIM4->CNT, value); break;
    case ENC4: encoder_hw_set(&TIM2->CNT, value); break;
    case ENC3:
#if ENC_CFG_MODE_ENC3 == ENC_MODE_SOFT_QUADRATURE
        enc3_count = value;
#elif ENC_CFG_MODE_ENC3 == ENC_MODE_STEP_DIR
        enc3_step_count = value;
#endif
        break;
    default:
        break;
    }
}

/* -------------------------------------------------------------------------
 *  Gestion ISR quadrature / step-dir
 * ------------------------------------------------------------------------- */
#if ENC_CFG_MODE_ENC3 == ENC_MODE_SOFT_QUADRATURE
static void encoder3_update_irq(void) {
    const uint8_t a = palReadLine(LINE_ENC3_A);
    const uint8_t b = palReadLine(LINE_ENC3_B);
    const uint8_t state = (uint8_t)((a << 1) | b);

    static const int8_t table[16] = {
         0, -1, +1,  0,
        +1,  0,  0, -1,
        -1,  0,  0, +1,
         0, +1, -1,  0
    };

    const uint8_t idx = (uint8_t)((enc3_last << 2) | state);
    enc3_count += table[idx];
    enc3_last = state;
}

static void enc3_pal_cb(void *arg) {
    (void)arg;
    encoder3_update_irq();
}
#elif ENC_CFG_MODE_ENC3 == ENC_MODE_STEP_DIR
static void enc3_step_cb(void *arg) {
    (void)arg;
    const bool dir = palReadLine(LINE_ENC3_B) != 0;
    if (dir) {
        ++enc3_step_count;
    } else {
        --enc3_step_count;
    }
}
#endif

/* -------------------------------------------------------------------------
 *  Initialisation hardware
 * ------------------------------------------------------------------------- */
static void encoders_hw_init(void) {
    /* ENC1 : TIM8 */
    rccEnableTIM8(true);
    TIM8->SMCR  = TIM_SMCR_SMS_0 | TIM_SMCR_SMS_1;
    TIM8->CCMR1 = TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_0;
    TIM8->CCER  = 0U;
    TIM8->ARR   = 0xFFFFU;
    TIM8->CNT   = 0U;
    TIM8->CR1   = TIM_CR1_CEN;

    /* ENC2 : TIM4 */
    rccEnableTIM4(true);
    TIM4->SMCR  = TIM_SMCR_SMS_0 | TIM_SMCR_SMS_1;
    TIM4->CCMR1 = TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_0;
    TIM4->CCER  = 0U;
    TIM4->ARR   = 0xFFFFU;
    TIM4->CNT   = 0U;
    TIM4->CR1   = TIM_CR1_CEN;

    /* ENC4 : TIM2 */
    rccEnableTIM2(true);
    TIM2->SMCR  = TIM_SMCR_SMS_0 | TIM_SMCR_SMS_1;
    TIM2->CCMR1 = TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_0;
    TIM2->CCER  = 0U;
    TIM2->ARR   = 0xFFFFU;
    TIM2->CNT   = 0U;
    TIM2->CR1   = TIM_CR1_CEN;

#if ENC_CFG_MODE_ENC3 == ENC_MODE_SOFT_QUADRATURE
    const uint8_t a = palReadLine(LINE_ENC3_A);
    const uint8_t b = palReadLine(LINE_ENC3_B);
    enc3_last = (uint8_t)((a << 1) | b);
    enc3_count = 0;
#elif ENC_CFG_MODE_ENC3 == ENC_MODE_STEP_DIR
    enc3_step_count = 0;
#endif
}

/* -------------------------------------------------------------------------
 *  API publique
 * ------------------------------------------------------------------------- */
void drv_encoders_start(void) {
    encoders_hw_init();

    /* Reconfigure les callbacks suivant le mode ENC3 */
    palDisableLineEvent(LINE_ENC3_A);
    palDisableLineEvent(LINE_ENC3_B);
#if ENC_CFG_MODE_ENC3 == ENC_MODE_SOFT_QUADRATURE
    palSetLineCallback(LINE_ENC3_A, enc3_pal_cb, NULL);
    palSetLineCallback(LINE_ENC3_B, enc3_pal_cb, NULL);
    palEnableLineEvent(LINE_ENC3_A, PAL_EVENT_MODE_BOTH_EDGES);
    palEnableLineEvent(LINE_ENC3_B, PAL_EVENT_MODE_BOTH_EDGES);
#elif ENC_CFG_MODE_ENC3 == ENC_MODE_STEP_DIR
    palSetLineCallback(LINE_ENC3_A, enc3_step_cb, NULL);
    palSetLineCallback(LINE_ENC3_B, NULL, NULL);
    palEnableLineEvent(LINE_ENC3_A, PAL_EVENT_MODE_RISING_EDGE);
#endif

    const systime_t now = chVTGetSystemTimeX();
    for (unsigned i = 0; i < (unsigned)NUM_ENCODERS; ++i) {
        const encoder_id_t id = (encoder_id_t)i;
        const int16_t raw = encoder_read_raw(id);
        g_last_simple[i]      = raw;
        g_residual_ticks[i]   = 0;
        g_accel_state[i].last_raw   = raw;
        g_accel_state[i].last_time  = now;
        g_accel_state[i].ema_speed  = 0.0f;
        g_accel_state[i].accel_level= 0U;
        g_accel_state[i].last_sign  = 0;
    }
}

int16_t drv_encoder_get(encoder_id_t id) {
    if (!encoder_id_valid(id)) {
        return 0;
    }
    return encoder_read_raw(id);
}

void drv_encoder_reset(encoder_id_t id) {
    if (!encoder_id_valid(id)) {
        return;
    }

    encoder_write_raw(id, 0);
    g_residual_ticks[id] = 0;

    const systime_t now = chVTGetSystemTimeX();
    g_last_simple[id]            = 0;
    g_accel_state[id].last_raw   = 0;
    g_accel_state[id].last_time  = now;
    g_accel_state[id].ema_speed  = 0.0f;
    g_accel_state[id].accel_level= 0U;
    g_accel_state[id].last_sign  = 0;
}

int16_t drv_encoder_get_delta(encoder_id_t id) {
    if (!encoder_id_valid(id)) {
        return 0;
    }

    const int16_t current = encoder_read_raw(id);
    const int16_t delta_raw = (int16_t)(current - g_last_simple[id]);
    g_last_simple[id] = current;

    if (delta_raw == 0) {
        return 0;
    }

    return encoder_normalize_delta(id, delta_raw);
}

int16_t drv_encoder_get_delta_accel(encoder_id_t id) {
    if (!encoder_id_valid(id)) {
        return 0;
    }

    encoder_accel_state_t *st = &g_accel_state[id];
    const int16_t current = encoder_read_raw(id);
    const int16_t delta_raw = (int16_t)(current - st->last_raw);

    if (delta_raw == 0) {
        return 0;
    }

    st->last_raw = current;
    g_last_simple[id] = current;

    int16_t detents = encoder_normalize_delta(id, delta_raw);
    if (detents == 0) {
        return 0;
    }

    const int8_t sign = (detents > 0) ? 1 : -1;
    const systime_t now = chVTGetSystemTimeX();
    uint32_t dt_ms = TIME_I2MS(now - st->last_time);
    if (dt_ms == 0U) {
        dt_ms = 1U;
    }

    if ((sign != 0) && (sign != st->last_sign)) {
        st->ema_speed = 0.0f;
        st->accel_level = 0U;
    }

    if (dt_ms > ENC_ACCEL_IDLE_RESET_MS) {
        st->ema_speed = 0.0f;
        st->accel_level = 0U;
        dt_ms = ENC_ACCEL_IDLE_RESET_MS;
    }

    const int32_t magnitude = (detents >= 0) ? detents : -detents;
    const float inst_speed = ((float)magnitude * 1000.0f) / (float)dt_ms;
    const float tau = (ENC_ACCEL_TAU_MS < 1.0f) ? 1.0f : ENC_ACCEL_TAU_MS;
    const float alpha = (float)dt_ms / (tau + (float)dt_ms);

    st->ema_speed += alpha * (inst_speed - st->ema_speed);
    st->accel_level = encoder_update_level(st->accel_level, st->ema_speed);
    st->last_time = now;
    st->last_sign = sign;

    int32_t mult = 1;
    if (st->accel_level >= 2U) {
        mult = ENC_ACCEL_LEVEL2_GAIN;
    } else if (st->accel_level == 1U) {
        mult = ENC_ACCEL_LEVEL1_GAIN;
    }

    if (mult > ENC_ACCEL_MAX_GAIN) {
        mult = ENC_ACCEL_MAX_GAIN;
    }

    const int32_t accelerated = (int32_t)detents * mult;
    return saturate_to_int16(accelerated);
}
