/**
 * @file cart_registry.c
 * @brief Implémentation du registre de cartouches Brick.
 * @ingroup cart
 */

#include "cart_registry.h"
#include "ui/ui_spec.h"   /* taille/forme de struct ui_cart_spec_t */
#include <stddef.h>       /* NULL */

static const struct ui_cart_spec_t* s_ui_specs[CART_COUNT] = { 0 };
static cart_id_t s_active_id = CART1;

void cart_registry_init(void) {
    for (cart_id_t i = 0; i < CART_COUNT; ++i) s_ui_specs[i] = NULL;
    s_active_id = CART1;
}

void cart_registry_register(cart_id_t id, const struct ui_cart_spec_t* ui_spec) {
    if (id >= CART_COUNT) return;
    s_ui_specs[id] = ui_spec;
}

const struct ui_cart_spec_t* cart_registry_get_ui_spec(cart_id_t id) {
    if (id >= CART_COUNT) return NULL;
    return s_ui_specs[id];
}

const struct ui_cart_spec_t* cart_registry_switch(cart_id_t id) {
    if (id >= CART_COUNT) return s_ui_specs[s_active_id];
    s_active_id = id;
    return s_ui_specs[s_active_id];
}

cart_id_t cart_registry_get_active_id(void) {
    return s_active_id;
}

bool cart_registry_is_present(cart_id_t id) {
    if (id >= CART_COUNT) return false;
    if (id >= CART_COUNT) return false;
    return s_ui_specs[id] != NULL;
     }
