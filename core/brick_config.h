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

#include "ccmattr.h"

/* -----------------------------------------------------------------------
 * Experimental feature toggles
 * ----------------------------------------------------------------------- */
#ifndef BRICK_EXPERIMENTAL_PATTERN_CODEC_V2
#define BRICK_EXPERIMENTAL_PATTERN_CODEC_V2 0
#endif

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
#define ENC_TICKS_PER_STEP   8        /**< Nombre de ticks matériels par pas logique. */
#define ENC_ACCEL_TAU_MS     120.0f   /**< Constante de temps EMA pour la vitesse. */
#define ENC_ACCEL_V0         50.0f    /**< Seuil de début d’accélération. */
#define ENC_ACCEL_V1         300.0f   /**< Seuil haut de la zone accélérée. */
#define ENC_ACCEL_G1         0.010f   /**< Gain de pente pour zone moyenne. */
#define ENC_ACCEL_G2         0.003f   /**< Gain de pente pour zone haute. */
#define ENC_ACCEL_MAX        10.0f    /**< Multiplicateur maximum. */

#define ENC_FLICK_THRESH     600.0f   /**< Seuil de détection d’un flick rapide. */
#define ENC_FLICK_GAIN       0.003f   /**< Gain ajouté lors d’un flick. */
#define ENC_FLICK_TAU_MS     150.0f   /**< Durée de décroissance du flick. */

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
