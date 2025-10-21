/**
 * @file brick_config.h
 * @brief Configuration matérielle et paramètres globaux du firmware **Brick**.
 *
 * Ce fichier centralise toutes les constantes liées :
 * - à la configuration matérielle (boutons, LEDs, ADC, etc.)
 * - au comportement de l’interface (timings, threads)
 * - aux profils de sensibilité des encodeurs
 * - aux options de débogage et d’identification du firmware
 *
 * @ingroup config
 */

#ifndef BRICK_CONFIG_H
#define BRICK_CONFIG_H

/* -----------------------------------------------------------------------
 * Experimental feature toggles
 * ----------------------------------------------------------------------- */
#ifndef BRICK_EXPERIMENTAL_PATTERN_CODEC_V2
#define BRICK_EXPERIMENTAL_PATTERN_CODEC_V2 0
#endif

#ifdef CCM_DATA
#undef CCM_DATA
#endif
#define CCM_DATA

/* =======================================================================
 *  Informations générales
 * ======================================================================= */
#define BRICK_FIRMWARE_VERSION   "0.9.3"
#define BRICK_DEVICE_NAME        "Brick Synth Engine"

/* =======================================================================
 *   Configuration matérielle globale
 * ======================================================================= */
#define NUM_BUTTONS       40   /**< Nombre total de boutons. */
#define NUM_ENCODERS      4    /**< Nombre total d’encodeurs rotatifs. */
#define NUM_POTS          4    /**< Nombre total de potentiomètres analogiques. */
#define NUM_ADRESS_LEDS  17    /**< Nombre de LEDs adressables (WS2812/SK6812). */
#define NUM_GPIO_LEDS     8    /**< LEDs simples sur GPIO (optionnel). */

/* =======================================================================
 *  LEDs adressables
 * ======================================================================= */
#define LED_BRIGHTNESS    32          /**< Intensité globale (0–255). */
#define LED_MODE_DEFAULT  LED_MODE_ON /**< Mode par défaut. */

/* =======================================================================
 *   Encodeurs : profil d’accélération et “flick”
 * ======================================================================= */
#define ENC_TICKS_PER_STEP   4

#define ENC_ACCEL_TAU_MS     50.0f
#define ENC_ACCEL_V0         100.0f
#define ENC_ACCEL_V1         200.0f
#define ENC_ACCEL_G1         0.080f
#define ENC_ACCEL_G2         0.025f
#define ENC_ACCEL_MAX        36.0f

#define ENC_FLICK_THRESH     100.0f
#define ENC_FLICK_GAIN       0.010f
#define ENC_FLICK_TAU_MS     100.0f


/* =======================================================================
 *   Threads & Timings de l’interface utilisateur
 * ======================================================================= */
#define UI_FRAME_INTERVAL_MS   16   /**< Intervalle d’affichage (≈ 60 FPS). */
#define UI_INPUT_POLL_MS       20   /**< Fréquence de polling des entrées. */
#define LED_RENDER_INTERVAL_MS 20   /**< Périodicité du rendu LED. */

/* =======================================================================
 *   Cart bus
 * ======================================================================= */
#define CART_BUS_TIMEOUT_MS  50
#define CART_LINK_DEFAULT    CART1

/* =======================================================================
 *   Debug et journalisation
 * ======================================================================= */
#define DEBUG_ENABLE     0
#define DEBUG_UART_BAUD  115200

#if DEBUG_ENABLE
  #define debug_log(fmt, ...) chprintf((BaseSequentialStream*)&SD2, "[DBG] " fmt "\r\n", ##__VA_ARGS__)
#else
  #define debug_log(fmt, ...) ((void)0)
#endif

#endif /* BRICK_CONFIG_H */
