/**
 * @file drv_leds_addr.h
 * @brief Interface du driver pour LEDs adressables **SK6812 / WS2812**.
 *
 * Ce module gère un ruban ou une matrice de LEDs RGB adressables connectées
 * sur une seule ligne de sortie (WS2812-like).
 *
 * Fonctionnalités principales :
 * - Gestion logicielle du protocole **800 kHz GRB**
 * - Définition d’un buffer mémoire `led_buffer[]` pour le rendu
 * - Couleurs prédéfinies (RGB standard + teintes utiles)
 * - Modes logiques (ON, OFF, BLINK, PLAYHEAD)
 * - Fonction `drv_leds_addr_render()` pour le rendu automatique
 *
 * @note Compatible avec les modèles **WS2812B**, **SK6812** et compatibles.
 *
 * @ingroup drivers
 */

#ifndef DRV_LEDS_ADDR_H
#define DRV_LEDS_ADDR_H

#include "ch.h"
#include "brick_config.h"
#include "hal.h"
#include <stdint.h>

/* =======================================================================
 *                              TYPES & COULEURS
 * ======================================================================= */

/**
 * @brief Structure de couleur GRB (format natif WS2812/SK6812).
 */
typedef struct {
    uint8_t g;  /**< Canal vert */
    uint8_t r;  /**< Canal rouge */
    uint8_t b;  /**< Canal bleu */
} led_color_t;

/* === Couleurs prédéfinies === */
#define COLOR_RED       (led_color_t){0, 255, 0}
#define COLOR_GREEN     (led_color_t){255, 0, 0}
#define COLOR_BLUE      (led_color_t){0, 0, 255}
#define COLOR_YELLOW    (led_color_t){255, 255, 0}
#define COLOR_CYAN      (led_color_t){255, 0, 255}
#define COLOR_MAGENTA   (led_color_t){0, 255, 255}
#define COLOR_WHITE     (led_color_t){255, 255, 255}

/* === Couleurs supplémentaires === */
#define COLOR_ORANGE    (led_color_t){128, 255, 0}
#define COLOR_PINK      (led_color_t){20, 255, 127}
#define COLOR_PURPLE    (led_color_t){0, 128, 255}
#define COLOR_TURQUOISE (led_color_t){255, 64, 128}
#define COLOR_OFF       (led_color_t){0, 0, 0}

/* =======================================================================
 *                              MAPPAGE LOGIQUE
 * ======================================================================= */

/* Mapping LEDs → boutons physiques Brick */
#define LED_REC        0
#define LED_SEQ8       1
#define LED_SEQ7       2
#define LED_SEQ6       3
#define LED_SEQ5       4
#define LED_SEQ4       5
#define LED_SEQ3       6
#define LED_SEQ2       7
#define LED_SEQ1       8
#define LED_SEQ9       9
#define LED_SEQ10      10
#define LED_SEQ11      11
#define LED_SEQ12      12
#define LED_SEQ13      13
#define LED_SEQ14      14
#define LED_SEQ15      15
#define LED_SEQ16      16

/* =======================================================================
 *                              MODES LOGIQUES
 * ======================================================================= */

/**
 * @brief Mode d’affichage d’une LED.
 */
typedef enum {
    LED_MODE_OFF,       /**< Éteinte */
    LED_MODE_ON,        /**< Allumée en continu */
    LED_MODE_BLINK,     /**< Clignotement périodique (~2 Hz) */
    LED_MODE_PLAYHEAD   /**< Effet pulsé (utilisé pour repère de lecture) */
} led_mode_t;

/**
 * @brief Structure d’état logique d’une LED.
 */
typedef struct {
    led_color_t color;  /**< Couleur de base */
    led_mode_t  mode;   /**< Mode d’affichage */
} led_state_t;

/** @brief État logique global de chaque LED. */
extern led_state_t drv_leds_addr_state[NUM_ADRESS_LEDS];

/* =======================================================================
 *                              API MATÉRIELLE
 * ======================================================================= */

/**
 * @brief Initialise la ligne de sortie et réinitialise les LEDs.
 */
void drv_leds_addr_init(void);

/**
 * @brief Envoie le contenu du buffer vers les LEDs (protocole GRB 800 kHz).
 */
void drv_leds_addr_update(void);

/**
 * @brief Définit la couleur d’une LED en composantes RGB.
 * @param index Indice de la LED [0–NUM_ADRESS_LEDS-1]
 * @param r Rouge (0–255)
 * @param g Vert (0–255)
 * @param b Bleu (0–255)
 */
void drv_leds_addr_set_rgb(int index, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Définit la couleur d’une LED via une structure `led_color_t`.
 */
void drv_leds_addr_set_color(int index, led_color_t color);

/**
 * @brief Éteint toutes les LEDs (buffer uniquement, sans envoi).
 */
void drv_leds_addr_clear(void);

/* =======================================================================
 *                              API LOGIQUE
 * ======================================================================= */

/**
 * @brief Définit l’état logique d’une LED (couleur + mode).
 */
void drv_leds_addr_set(int index, led_color_t color, led_mode_t mode);

/**
 * @brief Met à jour le buffer physique selon les états logiques (`drv_leds_addr_state[]`).
 *
 * Doit être appelée périodiquement (~30–60 Hz) pour actualiser les effets.
 */
void drv_leds_addr_render(void);

#endif /* DRV_LEDS_ADDR_H */
