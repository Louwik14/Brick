#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cart/cart_registry.h"
#include "core/seq/seq_access.h"
#include "core/seq/seq_project_access.h"
#include "core/seq/seq_config.h"
#include "ui/ui_spec.h"

static const ui_cart_spec_t k_stub_cart_specs[4] = {
    {
        .cart_name = "XVA1-1",
        .overlay_tag = NULL,
        .menus = {{0}},
        .cycles = {{0}},
    },
    {
        .cart_name = "XVA1-2",
        .overlay_tag = NULL,
        .menus = {{0}},
        .cycles = {{0}},
    },
    {
        .cart_name = "XVA1-3",
        .overlay_tag = NULL,
        .menus = {{0}},
        .cycles = {{0}},
    },
    {
        .cart_name = "XVA1-4",
        .overlay_tag = NULL,
        .menus = {{0}},
        .cycles = {{0}},
    },
};

static void register_stub_carts(void) {
    cart_registry_init();
    cart_registry_register(CART1, &k_stub_cart_specs[0]);
    cart_registry_register(CART2, &k_stub_cart_specs[1]);
    cart_registry_register(CART3, &k_stub_cart_specs[2]);
    cart_registry_register(CART4, &k_stub_cart_specs[3]);
}

static void prepare_project(seq_project_t *project,
                            seq_model_track_t *tracks,
                            uint8_t active_carts) {
    assert(active_carts >= 1U && active_carts <= 4U);

    seq_project_init(project);

    const uint16_t tracks_per_cart = XVA1_TRACKS_PER_CART;
    const uint16_t total_tracks = (uint16_t)active_carts * tracks_per_cart;

    for (uint16_t idx = 0U; idx < total_tracks; ++idx) {
        const uint8_t cart_index = (uint8_t)(idx / tracks_per_cart);
        const uint8_t local_index = (uint8_t)(idx % tracks_per_cart);
        (void)local_index;

        seq_project_assign_track(project, (uint8_t)idx, &tracks[idx]);
        seq_project_cart_ref_t ref = {
            .cart_id = 0x1000U + cart_index,
            .slot_id = cart_index,
            .capabilities = SEQ_PROJECT_CART_CAP_NONE,
            .flags = SEQ_PROJECT_CART_FLAG_NONE,
            .reserved = 0U,
        };
        seq_project_set_track_cart(project, (uint8_t)idx, &ref);
    }

    /* Ensure cached active track matches first track. */
    if (total_tracks > 0U) {
        (void)seq_project_set_active_track(project, 0U);
    }
}

static void build_render_string(const seq_project_t *project, char *dst, size_t dst_len) {
    dst[0] = '\0';
    const uint8_t cart_count = seq_project_get_cart_count(project);

    for (uint8_t cart = 0U; cart < cart_count; ++cart) {
        uint16_t start = 0U;
        uint16_t count = 0U;
        if (!seq_project_get_cart_track_span(project, cart, &start, &count)) {
            continue;
        }

        if (cart > 0U) {
            strncat(dst, "|", dst_len - strlen(dst) - 1U);
        }

        const char *name = seq_project_get_cart_name(project, cart);
        if ((name == NULL) || (name[0] == '\0')) {
            char fallback[8];
            (void)snprintf(fallback, sizeof(fallback), "CART%u", (unsigned)(cart + 1U));
            name = fallback;
        }
        strncat(dst, name, dst_len - strlen(dst) - 1U);
        strncat(dst, ":", dst_len - strlen(dst) - 1U);

        for (uint16_t k = 0U; k < count; ++k) {
            if (k > 0U) {
                strncat(dst, ",", dst_len - strlen(dst) - 1U);
            }
            char label[8];
            (void)snprintf(label, sizeof(label), "T%02u", (unsigned)(start + k + 1U));
            strncat(dst, label, dst_len - strlen(dst) - 1U);
        }
    }
}

int main(void) {
    register_stub_carts();

    for (uint8_t carts = 1U; carts <= 4U; ++carts) {
        seq_project_t project;
        memset(&project, 0, sizeof(project));
        seq_model_track_t tracks[SEQ_PROJECT_MAX_TRACKS];
        memset(tracks, 0, sizeof(tracks));

        prepare_project(&project, tracks, carts);

        const uint16_t expected_tracks = (uint16_t)carts * XVA1_TRACKS_PER_CART;
        assert(seq_project_get_track_count(&project) == expected_tracks);
        assert(seq_project_get_cart_count(&project) == carts);

        uint16_t covered = 0U;
        for (uint8_t cart = 0U; cart < carts; ++cart) {
            uint16_t start = 0U;
            uint16_t count = 0U;
            assert(seq_project_get_cart_track_span(&project, cart, &start, &count));
            assert(count == XVA1_TRACKS_PER_CART);
            assert(start == covered);
            covered = (uint16_t)(covered + count);

            const char *name = seq_project_get_cart_name(&project, cart);
            assert(name != NULL);
            char expected_name[8];
            (void)snprintf(expected_name, sizeof(expected_name), "XVA1-%u", (unsigned)(cart + 1U));
            assert(strcmp(name, expected_name) == 0);
        }
        assert(covered == expected_tracks);

        char rendered[256];
        build_render_string(&project, rendered, sizeof(rendered));

        char expected[256];
        expected[0] = '\0';
        for (uint8_t cart = 0U; cart < carts; ++cart) {
            if (cart > 0U) {
                strncat(expected, "|", sizeof(expected) - strlen(expected) - 1U);
            }
            char name[8];
            (void)snprintf(name, sizeof(name), "XVA1-%u", (unsigned)(cart + 1U));
            strncat(expected, name, sizeof(expected) - strlen(expected) - 1U);
            strncat(expected, ":", sizeof(expected) - strlen(expected) - 1U);
            for (uint16_t k = 0U; k < XVA1_TRACKS_PER_CART; ++k) {
                if (k > 0U) {
                    strncat(expected, ",", sizeof(expected) - strlen(expected) - 1U);
                }
                char label[8];
                (void)snprintf(label, sizeof(label), "T%02u", (unsigned)(cart * XVA1_TRACKS_PER_CART + k + 1U));
                strncat(expected, label, sizeof(expected) - strlen(expected) - 1U);
            }
        }

        assert(strcmp(rendered, expected) == 0);
    }

    return 0;
}
