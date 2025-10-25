/**
 * @file cart_registry.c
 * @brief Implémentation du registre de cartouches Brick.
 * @ingroup cart
 */

#include "cart_registry.h"
#include "core/cart/cart_registry_access.h"
#include "ui/ui_spec.h"   /* taille/forme de struct ui_cart_spec_t */
#include <stddef.h>       /* NULL */
#include <stdio.h>
#include <string.h>

static const struct ui_cart_spec_t* s_ui_specs[CART_COUNT] = { 0 };
static uint32_t s_cart_uid[CART_COUNT];
static cart_id_t s_active_id = CART1;

void cart_registry_init(void) {
    for (cart_id_t i = 0; i < CART_COUNT; ++i) {
        s_ui_specs[i] = NULL;
        s_cart_uid[i] = 0U;
    }
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
    return s_ui_specs[id] != NULL;
}

void cart_registry_set_uid(cart_id_t id, uint32_t uid) {
    if (id >= CART_COUNT) {
        return;
    }
    s_cart_uid[id] = uid;
}

uint32_t cart_registry_get_uid(cart_id_t id) {
    if (id >= CART_COUNT) {
        return 0U;
    }
    return s_cart_uid[id];
}

bool cart_registry_find_by_uid(uint32_t uid, cart_id_t *out_id) {
    if (uid == 0U) {
        return false;
    }
    for (cart_id_t i = 0; i < CART_COUNT; ++i) {
        if (s_cart_uid[i] == uid) {
            if (out_id != NULL) {
                *out_id = i;
            }
            return true;
        }
    }
    return false;
}

bool cart_registry_cart_name(uint8_t cart_index, char *out, uint8_t out_len) {
    if ((out == NULL) || (out_len == 0U)) {
        return false;
    }
    out[0] = '\0';
    if (cart_index >= (uint8_t)CART_COUNT) {
        return false;
    }

    const struct ui_cart_spec_t *spec = cart_registry_get_ui_spec((cart_id_t)cart_index);
    if ((spec != NULL) && (spec->cart_name != NULL) && (spec->cart_name[0] != '\0')) {
        size_t len = strlen(spec->cart_name);
        if (len >= out_len) {
            len = (size_t)out_len - 1U;
        }
        memcpy(out, spec->cart_name, len);
        out[len] = '\0';
        return true;
    }

    const int written = snprintf(out, (size_t)out_len, "CART%u", (unsigned)(cart_index + 1U));
    return (written > 0) && ((size_t)written < (size_t)out_len);
}
