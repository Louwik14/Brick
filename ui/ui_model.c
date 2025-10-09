/**
 * @file ui_model.c
 * @brief État interne mutable de l’UI Brick (copie RAM + cart active).
 * @ingroup ui
 */

#include "ui_model.h"
#include <string.h>

/* Cartouche actuellement affichée (spec UI). */
static const ui_cart_spec_t *s_cart_active = NULL;
/* Cartouche précédente (pile 1 niveau). */
static const ui_cart_spec_t *s_cart_last   = NULL;
/* État courant. */
static ui_state_t s_ui_state;

void ui_state_init(ui_state_t *st, const ui_cart_spec_t *spec) {
    st->spec     = spec;
    st->cur_menu = 0;
    st->cur_page = 0;
    st->shift    = false;

    memset(&st->vals, 0, sizeof(st->vals));
    if (!spec) return;

    /* Copie des valeurs par défaut (alignées sur la spec). */
    for (int m = 0; m < UI_MODEL_MAX_MENUS; m++) {
        for (int p = 0; p < UI_MODEL_MAX_PAGES; p++) {
            for (int i = 0; i < UI_MODEL_PARAMS_PER_PAGE; i++) {
                const ui_param_spec_t *ps = &spec->menus[m].pages[p].params[i];
                ui_param_state_t      *pv = &st->vals.menus[m].pages[p].params[i];
                pv->value = ps->label ? ps->default_value : 0;
            }
        }
    }
}

void ui_model_switch_cart(const ui_cart_spec_t *spec) {
    if (!spec || spec == s_cart_active) return;
    s_cart_last   = s_cart_active;
    s_cart_active = spec;
    ui_state_init(&s_ui_state, spec);
}

void ui_model_restore_last_cart(void) {
    if (!s_cart_last) return;
    const ui_cart_spec_t *tmp = s_cart_active;
    s_cart_active = s_cart_last;
    s_cart_last   = tmp;
    ui_state_init(&s_ui_state, s_cart_active);
}

void ui_model_init(const ui_cart_spec_t* initial_spec) {
    s_cart_last   = NULL;
    s_cart_active = initial_spec;
    ui_state_init(&s_ui_state, initial_spec);
    /* Par défaut, les boutons step sont en mode SEQ → tag persistant "SEQ" */
    ui_model_set_active_overlay_tag("SEQ");

}

const ui_cart_spec_t *ui_model_get_active_spec(void) { return s_cart_active; }
ui_state_t *ui_model_get_state(void) { return &s_ui_state; }
static char g_last_overlay_tag[8] = "";

void ui_model_set_active_overlay_tag(const char *tag) {
    if (tag && tag[0])
        strncpy(g_last_overlay_tag, tag, sizeof(g_last_overlay_tag)-1);
    else
        g_last_overlay_tag[0] = '\0';
}

const char *ui_model_get_active_overlay_tag(void) {
    if (g_last_overlay_tag[0] == '\0') {
        /* Valeur par défaut au boot : SEQ */
        strcpy(g_last_overlay_tag, "SEQ");
    }
    return g_last_overlay_tag;
}
