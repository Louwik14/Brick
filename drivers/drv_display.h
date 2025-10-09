/**
 * @file drv_display.h
 * @brief Interface du driver OLED SPI **SSD1309 (128×64)** pour Brick.
 *
 * Ce module fournit une API haut niveau pour le rendu graphique sur écran OLED :
 * - Initialisation du contrôleur SSD1309 (via SPI1)
 * - Gestion d’un framebuffer mémoire local
 * - Fonctions de dessin de texte et caractères
 * - Support de polices configurables (`font_t`)
 * - Thread optionnel de rafraîchissement automatique (~30 FPS)
 *
 * @note Le framebuffer doit être explicitement mis à jour via `drv_display_update()`
 *       si le thread d’affichage n’est pas démarré.
 *
 * @ingroup drivers
 */

#ifndef DRV_DISPLAY_H
#define DRV_DISPLAY_H

#include "ch.h"
#include "brick_config.h"
#include "hal.h"
#include <stdint.h>
#include "font.h"

/* ====================================================================== */
/*                           CONFIGURATION GÉNÉRALE                       */
/* ====================================================================== */

/** @brief Largeur en pixels de l’écran OLED. */
#define OLED_WIDTH   128

/** @brief Hauteur en pixels de l’écran OLED. */
#define OLED_HEIGHT  64

/* ====================================================================== */
/*                              FONCTIONS API                             */
/* ====================================================================== */

/**
 * @brief Initialise le contrôleur OLED et le framebuffer.
 *
 * Configure le bus SPI, le SSD1309 et charge la police par défaut (`FONT_5X7`).
 */
void drv_display_init(void);

/**
 * @brief Efface tout le contenu du framebuffer.
 */
void drv_display_clear(void);

/**
 * @brief Transfère le contenu du framebuffer vers l’écran OLED.
 *
 * À appeler périodiquement si le thread automatique n’est pas utilisé.
 */
void drv_display_update(void);

/**
 * @brief Retourne un pointeur vers le framebuffer local (1 bit/pixel).
 * @return Pointeur sur le buffer 128×64 / 8 = 1024 octets.
 */
uint8_t* drv_display_get_buffer(void);

/**
 * @brief Démarre le thread d’affichage automatique (~30 FPS).
 *
 * Initialise l’écran et lance un thread dédié au rafraîchissement.
 */
void drv_display_start(void);

/* ====================================================================== */
/*                           GESTION DES POLICES                          */
/* ====================================================================== */

/**
 * @brief Définit la police courante utilisée pour le rendu texte.
 * @param font Pointeur vers la structure de police à utiliser.
 */
void drv_display_set_font(const font_t *font);

/* ====================================================================== */
/*                           DESSIN ET RENDU TEXTE                        */
/* ====================================================================== */

/**
 * @brief Dessine un caractère ASCII à la position spécifiée.
 * @param x Position horizontale en pixels.
 * @param y Position verticale en pixels.
 * @param c Caractère à afficher.
 */
void drv_display_draw_char(uint8_t x, uint8_t y, char c);

/**
 * @brief Affiche une chaîne de caractères à partir d’une position donnée.
 * @param x Coordonnée X du premier caractère.
 * @param y Coordonnée Y du texte.
 * @param txt Pointeur vers la chaîne à afficher (terminée par `\0`).
 */
void drv_display_draw_text(uint8_t x, uint8_t y, const char *txt);

/**
 * @brief Affiche un nombre entier sous forme décimale.
 * @param x Coordonnée X du texte.
 * @param y Coordonnée Y du texte.
 * @param num Valeur entière à afficher.
 */
void drv_display_draw_number(uint8_t x, uint8_t y, int num);

/* ====================================================================== */
/*                      VARIANTES DE RENDU AVANCÉES                       */
/* ====================================================================== */

/**
 * @brief Dessine du texte avec une police spécifique sans changer la police globale.
 * @param font Police à utiliser pour ce rendu.
 * @param x Coordonnée X du texte.
 * @param y Coordonnée Y du texte.
 * @param txt Texte à afficher.
 */
void drv_display_draw_text_with_font(const font_t *font, uint8_t x, uint8_t y, const char *txt);

/**
 * @brief Dessine du texte aligné sur une ligne de base commune.
 *
 * Utile pour aligner verticalement plusieurs polices différentes.
 *
 * @param font Police à utiliser.
 * @param x Coordonnée X de départ.
 * @param baseline_y Position verticale de la ligne de base.
 * @param txt Texte à afficher.
 */
void drv_display_draw_text_at_baseline(const font_t *font,
                                       uint8_t x, uint8_t baseline_y,
                                       const char *txt);

/**
 * @brief Dessine un caractère centré dans une boîte rectangulaire.
 *
 * @param font Police utilisée.
 * @param x Coin supérieur gauche de la boîte (X).
 * @param y Coin supérieur gauche de la boîte (Y).
 * @param box_w Largeur de la boîte.
 * @param box_h Hauteur de la boîte.
 * @param c Caractère à afficher.
 */
void drv_display_draw_char_in_box(const font_t *font,
                                  uint8_t x, uint8_t y,
                                  uint8_t box_w, uint8_t box_h, char c);

#endif /* DRV_DISPLAY_H */
