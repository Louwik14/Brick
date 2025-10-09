/**
 * @file ui_icons.c
 * @brief Banque d’icônes 20x14 et conventions pour le mapping texte → icône.
 *
 * @ingroup ui
 *
 * @details
 * Ce module contient :
 * - les bitmaps d’icônes (20x14) utilisés par l’UI,
 * - les exports d’icônes sous forme de `const ui_icon_t UI_ICON_*`.
 *
 * L’UI ne dépend jamais d’indices d’énumération. Les icônes sont choisies
 * par texte via `uiw_draw_icon_by_text()` (implémenté dans `ui_widgets.c`),
 * qui applique un mappage texte → icône (familles : waves, filters, on/off).
 *
 * @section icons_format Format des icônes
 * - Dimensions attendues : 20x14 pixels (monochrome).
 * - Chaque icône est déclarée comme `extern const ui_icon_t UI_ICON_*;`
 *   et définie via la macro `UIW_ICON_DEFINE(...)` dans ce fichier.
 *
 * @section icons_naming Convention de nommage
 * - Préfixe obligatoire : UI_ICON_.
 * - Nom en MAJUSCULES, caractères non-alphanumériques convertis en '_'.
 *   - Ex. : "off" → UI_ICON_OFF
 *   - Ex. : "S+S2" → UI_ICON_S_S2
 *
 * @section icons_tool Génération rapide (script)
 * Utilisez `icon_converter.py` pour convertir une image en macro `UIW_ICON_DEFINE` :
 * @code{.bash}
 * python icon_converter.py S+S2.png S+S3.png --width 20 --height 14 --invert
 * # Copiez/collez la sortie ici, dans ui_icons.c
 * @endcode
 *
 * Exemple de sortie attendue (extrait) :
 * @code{.c}
 * UIW_ICON_DEFINE(UI_ICON_S_S2,
 *     0b00000000000000000000,
 *     ...,
 *     0b00000000000000000000
 * );
 * @endcode
 *
 * @section icons_mapping Mapping texte → icône (vue d’ensemble)
 * Le mapping est réalisé dans `ui_widgets.c` (fonction interne `match_icon_for_text()`).
 * La normalisation du label applique :
 * - passage en minuscules,
 * - suppression des espaces, '_' et '-'.
 *
 * Exemples de tokens reconnus :
 * - Waves : sine / square / triangle / saw / sawu / sawd / noise
 * - Filters : lp / hp / bp / notch
 * - Booléens : on / off (si les icônes correspondantes existent)
 *
 * @section icons_add_custom Ajouter des icônes pour des labels spécifiques (ex. "S+S2", "S+S3")
 * 1) Définir vos bitmaps ici, dans `ui_icons.c` :
 * @code{.c}
 * // UIW_ICON_DEFINE(UI_ICON_S2, <bitmap 20x14>);
 * // UIW_ICON_DEFINE(UI_ICON_S3, <bitmap 20x14>);
 * @endcode
 *
 * 2) Étendre le mapping dans `ui_widgets.c` (fonction `match_icon_for_text()`), après
 *    la normalisation du label :
 * @code{.c}
 * // "S+S2" → "ss2" ; "S+S3" → "ss3" après normalisation
 * if (key_eq(key, "ss2")) return &UI_ICON_S2;
 * if (key_eq(key, "ss3")) return &UI_ICON_S3;
 * @endcode
 *
 * @section icons_bool Icônes ON/OFF
 * - Si `UI_ICON_ON` et `UI_ICON_OFF` sont définies ici, le widget booléen
 *   (`uiw_draw_switch`) les utilise (pas de fallback texte).
 * - Si elles sont absentes, aucun dessin n’est produit pour les booléens.
 *
 * @section icons_guidelines Bonnes pratiques
 * - Conserver 20x14 pour garantir l’alignement/centrage.
 * - Préparer les assets en niveaux de gris et utiliser `--invert` si nécessaire
 *   pour obtenir du "blanc sur noir".
 * - Éviter les détails trop fins (14 px de haut).
 */

#include "ui_icons.h"
#include "drv_display.h"

/* ----------------------------------------------------------
 *   Icônes binaires (migrées telles quelles, bit-à-bit)
 * ---------------------------------------------------------- */

/* --- Onde sinusoïdale --- */
UIW_ICON_DEFINE(UI_ICON_SINE,
    0b00011110000000000000, 0b00110011000000000000, 0b01100001100000000000,
    0b01000000100000000000, 0b11000000110000000000, 0b10000000010000000000,
    0b10000000010000000000, 0b00000000001000000001, 0b00000000001000000001,
    0b00000000001100000011, 0b00000000000100000010, 0b00000000000110000110,
    0b00000000000011001100, 0b00000000000001111000
);

/* --- Carré --- */
UIW_ICON_DEFINE(UI_ICON_SQUARE,
    0b11111111110000000000, 0b10000000010000000000, 0b10000000010000000000,
    0b10000000010000000000, 0b10000000010000000000, 0b10000000010000000000,
    0b10000000010000000010, 0b00000000010000000010, 0b00000000010000000010,
    0b00000000010000000010, 0b00000000010000000010, 0b00000000010000000010,
    0b00000000010000000010, 0b00000000011111111110
);

/* --- Saw / SawU / SawD --- */
UIW_ICON_DEFINE(UI_ICON_SAW,   /* descendante */
    0b00000000000000000000, 0b00000000000000000000,
    0b00000000000000000111, 0b00000000000000011101,
    0b00000000000001110001, 0b00000000000111000001,
    0b00000000011100000001, 0b00000001110000000001,
    0b00000111000000000001, 0b00011100000000000001,
    0b01110000000000000001, 0b11000000000000000001,
    0b00000000000000000000, 0b00000000000000000000
);

UIW_ICON_DEFINE(UI_ICON_SAWD,  /* montante */
    0b00000000000000000000, 0b00000000000000000000,
    0b00000000000000000000, 0b00000000000000000000,
    0b11000000000000000001, 0b01110000000000000001,
    0b00011100000000000001, 0b00000111000000000001,
    0b00000001110000000001, 0b00000000011100000001,
    0b00000000000111000001, 0b00000000000001110001,
    0b00000000000000011101, 0b00000000000000000111
);

UIW_ICON_DEFINE(UI_ICON_SAWU,  /* descendante inverse */
    0b00000000000000000000, 0b00000000000000000000,
    0b00000000000000000111, 0b00000000000000011101,
    0b00000000000001110001, 0b00000000000111000001,
    0b00000000011100000001, 0b00000001110000000001,
    0b00000111000000000001, 0b00011100000000000001,
    0b01110000000000000001, 0b11000000000000000001,
    0b00000000000000000000, 0b00000000000000000000
);

/* --- Triangle --- */
UIW_ICON_DEFINE(UI_ICON_TRIANGLE,
    0b00000000010000000000, 0b00000000101000000000,
    0b00000001000100000000, 0b00000010000010000000,
    0b00000100000001000000, 0b00001000000000100000,
    0b00010000000000010000, 0b00100000000000001000,
    0b01000000000000000100, 0b10000000000000000010,
    0b00000000000000000001, 0b00000000000000000000,
    0b00000000000000000000, 0b00000000000000000000
);

/* --- Bruit (damier) --- */
UIW_ICON_DEFINE(UI_ICON_NOISE,
    0b10101010101010101010, 0b01010101010101010101,
    0b10101010101010101010, 0b01010101010101010101,
    0b10101010101010101010, 0b01010101010101010101,
    0b10101010101010101010, 0b01010101010101010101,
    0b10101010101010101010, 0b01010101010101010101,
    0b10101010101010101010, 0b01010101010101010101,
    0b10101010101010101010, 0b01010101010101010101
);

/* --- Filtres (LP / HP / BP / Notch) --- */
UIW_ICON_DEFINE(UI_ICON_LP,
    0b11111111111111111111, 0b10000000000000000000, 0b10000000000000000000,
    0b10000000000000000000, 0b10000000000000000000, 0b10000000000000000000,
    0b11111111111111111111, 0b00000000000000000000, 0b00000000000000000000,
    0b00000000000000000000, 0b00000000000000000000, 0b00000000000000000000,
    0b00000000000000000000, 0b00000000000000000000
);

UIW_ICON_DEFINE(UI_ICON_HP,
    0b11111111111111111111, 0b00000000000000000001, 0b00000000000000000001,
    0b00000000000000000001, 0b00000000000000000001, 0b00000000000000000001,
    0b11111111111111111111, 0b00000000000000000000, 0b00000000000000000000,
    0b00000000000000000000, 0b00000000000000000000, 0b00000000000000000000,
    0b00000000000000000000, 0b00000000000000000000
);

UIW_ICON_DEFINE(UI_ICON_BP,
    0b00000000000000000000, 0b00000111111111111000, 0b00000111111111111000,
    0b00000111111111111000, 0b00000111111111111000, 0b00000111111111111000,
    0b11111111111111111111, 0b00000111111111111000, 0b00000111111111111000,
    0b00000111111111111000, 0b00000111111111111000, 0b00000111111111111000,
    0b00000000000000000000, 0b00000000000000000000
);

UIW_ICON_DEFINE(UI_ICON_NOTCH,
    0b11111111111111111111, 0b11110000000000011111, 0b11110000000000011111,
    0b11110000000000011111, 0b11110000000000011111, 0b11110000000000011111,
    0b11110000000000011111, 0b11110000000000011111, 0b11110000000000011111,
    0b11110000000000011111, 0b11110000000000011111, 0b11111111111111111111,
    0b00000000000000000000, 0b00000000000000000000
);

UIW_ICON_DEFINE(UI_ICON_OFF,
    0b00000000000000000000,
    0b00000000000000000000,
    0b00000000000000000000,
    0b00000000000000000000,
    0b00000000000000000000,
    0b00000000000000000000,
    0b11000000000000000110,
    0b01100000000000001100,
    0b00110000000000011000,
    0b00011100000001110000,
    0b00000111000111000000,
    0b00000001111100000000,
    0b00000000000000000000,
    0b00000000000000000000
);

UIW_ICON_DEFINE(UI_ICON_ON,
    0b00000000000000000000,
    0b00000001111100000000,
    0b00000111000111000000,
    0b00011100111001110000,
    0b00110001011100011000,
    0b01100010011110001100,
    0b11000010111110000110,
    0b01100010001110001100,
    0b00110001011100011000,
    0b00011100111001110000,
    0b00000111000111000000,
    0b00000001111100000000,
    0b00000000000000000000,
    0b00000000000000000000
);



/* === Rendu pixel-par-pixel exact === */
void ui_icon_draw(const ui_icon_t* icon, int x, int y, bool on) {
    if (!icon) return;
    uint8_t* fb = drv_display_get_buffer();
    for (int row = 0; row < UI_ICON_HEIGHT; ++row) {
        uint32_t bits = icon->data[row];
        for (int col = 0; col < UI_ICON_WIDTH; ++col) {
            if (bits & (1u << (UI_ICON_WIDTH - 1 - col))) {
                int px = x + col, py = y + row;
                if ((unsigned)px >= OLED_WIDTH || (unsigned)py >= OLED_HEIGHT) continue;
                int index = px + (py >> 3) * OLED_WIDTH;
                uint8_t mask = (uint8_t)(1u << (py & 7));
                if (on) fb[index] |= mask; else fb[index] &= (uint8_t)~mask;
            }
        }
    }
}
