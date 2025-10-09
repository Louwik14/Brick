/**
 * @file ui_knob.c
 * @brief Knob plein (remplissage angulaire 0..360°) avec clip au disque.
 *
 * @details
 * - Unipolaire : départ SUD (+90°), remplissage 0→360° (full à la valeur max).
 * - Bipolaire  : 0 au NORD (−90°), négatif vers l’EST (horaire), positif vers l’OUEST (anti-horaire), jusqu’à 180°.
 * - Compatible avec toute plage [min..max] (pas besoin de 0..255).
 * - Pas de débordement (clip strict dans le disque) et pas de “pixels fantômes”.
 * - Réglage fin du diamètre au pixel près via ui_knob_set_diameter_px().
 */

#include "ui_knob.h"
#include "drv_display.h"
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

/* ==========================================================================
 *                         CONFIG / ETAT GLOBAL (facultatif)
 * ==========================================================================
 */

/* Override optionnel du diamètre (px). 0 = désactivé (utiliser r fourni). */
static int g_knob_diameter_override_px = 16;

/**
 * @brief Fixe un diamètre explicite (px) pour TOUS les knobs (0 pour désactiver).
 *        Si défini, l’argument `r` passé à ui_draw_knob() est ignoré.
 */
void ui_knob_set_diameter_px(int d_px) {
    if (d_px < 0) d_px = 0;
    g_knob_diameter_override_px = d_px;
}

/* ==========================================================================
 *                         FRAMEBUFFER HELPERS
 * ==========================================================================
 */

static inline void set_pixel(int x, int y, bool on) {
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;
    uint8_t *buf = drv_display_get_buffer();
    const int index = x + (y >> 3) * OLED_WIDTH;
    const uint8_t mask = (uint8_t)(1u << (y & 7));
    if (on)  buf[index] |=  mask;
    else     buf[index] &= (uint8_t)~mask;
}

static void draw_circle_outline(int cx, int cy, int r) {
    int x = r, y = 0, err = 0;
    while (x >= y) {
        set_pixel(cx + x, cy + y, true);
        set_pixel(cx + y, cy + x, true);
        set_pixel(cx - y, cy + x, true);
        set_pixel(cx - x, cy + y, true);
        set_pixel(cx - x, cy - y, true);
        set_pixel(cx - y, cy - x, true);
        set_pixel(cx + y, cy - x, true);
        set_pixel(cx + x, cy - y, true);
        if (err <= 0) { y++; err += 2*y + 1; }
        if (err > 0)  { x--; err -= 2*x + 1; }
    }
}

/* ==========================================================================
 *                         ANGLE / ARC HELPERS
 * ==========================================================================
 */

static inline float clamp01f(float t) {
    if (t < 0.f) return 0.f;
    if (t > 1.f) return 1.f;
    return t;
}

static inline float wrap_0_2pi(float a) {
    const float TWO_PI = 6.28318531f;
    while (a < 0.f)      a += TWO_PI;
    while (a >= TWO_PI)  a -= TWO_PI;
    return a;
}

/* Test si l’angle a est dans l’arc [a0..a1] (sens direct), wrap géré */
static inline bool angle_in_arc(float a, float a0, float a1) {
    a  = wrap_0_2pi(a);
    a0 = wrap_0_2pi(a0);
    a1 = wrap_0_2pi(a1);
    if (a0 <= a1) return (a >= a0 && a <= a1);
    return (a >= a0 || a <= a1); /* arc qui traverse 0 */
}

/* Remplit tout le disque (plein) */
static inline void fill_full_disk_mask(int cx, int cy, int r) {
    if (r <= 0) return;
    const int r2 = r * r;
    for (int yy = -r; yy <= r; ++yy) {
        int y = cy + yy;
        if ((unsigned)y >= OLED_HEIGHT) continue;
        for (int xx = -r; xx <= r; ++xx) {
            int x = cx + xx;
            if ((unsigned)x >= OLED_WIDTH) continue;
            if (xx*xx + yy*yy <= r2) set_pixel(x, y, true);
        }
    }
}

/* Remplit un arc de disque strictement clipé :
   - centre (cx,cy), rayon r, angles a0..a1 (radians, sens direct)
   - centre toujours rempli si arc non nul (évite le “pixel mort”) */
static void fill_disk_arc_mask(int cx, int cy, int r, float a0, float a1) {
    if (r <= 0) return;
    const int r2 = r * r;

    /* Estimer si arc non nul (pour forcer le centre) */
    float len = wrap_0_2pi(a1 - a0);
    bool non_zero_arc = (len > 1e-3f);

    for (int yy = -r; yy <= r; ++yy) {
        int y = cy + yy;
        if ((unsigned)y >= OLED_HEIGHT) continue;
        for (int xx = -r; xx <= r; ++xx) {
            int x = cx + xx;
            if ((unsigned)x >= OLED_WIDTH) continue;

            const int d2 = xx*xx + yy*yy;
            if (d2 > r2) continue; /* hors disque */

            if (xx == 0 && yy == 0) {
                if (non_zero_arc) set_pixel(x, y, true); /* centre toujours ON si arc>0 */
                continue;
            }

            float a = atan2f((float)yy, (float)xx); /* [-π..+π] */
            if (angle_in_arc(a, a0, a1)) set_pixel(x, y, true);
        }
    }
}

/* ==========================================================================
 *                               PUBLIC API
 * ==========================================================================
 */

void ui_draw_knob(int cx, int cy, int r_in, int val, int vmin, int vmax) {
    if (vmax <= vmin) return;

    /* Override diamètre global si demandé */
    int r = r_in;
    if (g_knob_diameter_override_px > 0) {
        int d = g_knob_diameter_override_px;
        if (d < 2) d = 2;
        r = d / 2;
    }

    /* Clamp la valeur à la plage */
    if (val < vmin) val = vmin;
    if (val > vmax) val = vmax;

    /* Marge d’un pixel pour le remplissage (évite de “coller” l’outline) */
    int r_fill = (r > 1) ? (r - 1) : r;

    /* Références angulaires */
    const float PI     = 3.14159265f;
    const float TWO_PI = 6.28318531f;
    const float ANG_S  = +PI * 0.5f;  /* SUD  = +90° */
    const float ANG_N  = -PI * 0.5f;  /* NORD = -90° */

    /* Unipolaire vs bipolaire */
    if (vmin < 0 && vmax > 0) {
        /* ----------------------- Bipolaire -----------------------
           0 est au NORD. On remplit :
           - val > 0 : de NORD vers l’OUEST (anti-horaire) sur [0..π]
           - val < 0 : de NORD vers l’EST   (horaire)      sur [0..π]
        */
        if (val > 0) {
            float tpos = clamp01f((float)val / (float)vmax); /* 0..1 */
            if (tpos > 0.f) {
                float a0 = ANG_N;
                float a1 = a0 - tpos * PI;       /* anti-horaire → OUEST */
                fill_disk_arc_mask(cx, cy, r_fill, a1, a0);
            }
        } else if (val < 0) {
            float tneg = clamp01f((float)(-val) / (float)(-vmin)); /* 0..1 */
            if (tneg > 0.f) {
                float a0 = ANG_N;
                float a1 = a0 + tneg * PI;       /* horaire → EST */
                fill_disk_arc_mask(cx, cy, r_fill, a0, a1);
            }
        }
        /* val == 0 -> rien de rempli, outline seulement */
    } else {
        /* ----------------------- Unipolaire -----------------------
           Départ pile au SUD (ANG_S). Remplissage 0→360°.
           Cas spéciaux : t==0 -> rien ; t>=1 -> plein disque.
        */
        float t = clamp01f((float)(val - vmin) / (float)(vmax - vmin)); /* 0..1 */

        if (t <= 0.f) {
            /* rien à remplir */
        } else if (t >= 1.f) {
            fill_full_disk_mask(cx, cy, r_fill);
        } else {
            float a0 = ANG_S;
            float a1 = a0 + t * TWO_PI; /* sens direct (anti-horaire) */
            fill_disk_arc_mask(cx, cy, r_fill, a0, a1);
        }
    }

    /* Bordure circulaire nette */
    draw_circle_outline(cx, cy, r);
}
