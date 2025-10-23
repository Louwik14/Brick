#include "board/board_flash.h"
#include "cart/cart_registry.h"
#include "core/seq/runtime/seq_runtime_cold.h"

bool board_flash_init(void) { return true; }
bool board_flash_is_ready(void) { return true; }
uint32_t board_flash_get_capacity(void) { return BOARD_FLASH_CAPACITY_BYTES; }
uint32_t board_flash_get_sector_size(void) { return BOARD_FLASH_SECTOR_SIZE; }
bool board_flash_read(uint32_t address, void *buffer, size_t length) {
    (void)address;
    (void)buffer;
    (void)length;
    return true;
}
bool board_flash_write(uint32_t address, const void *data, size_t length) {
    (void)address;
    (void)data;
    (void)length;
    return true;
}
bool board_flash_erase(uint32_t address, size_t length) {
    (void)address;
    (void)length;
    return true;
}
bool board_flash_erase_sector(uint32_t address) {
    (void)address;
    return true;
}

void cart_registry_init(void) {}
void cart_registry_register(cart_id_t id, const struct ui_cart_spec_t *spec) {
    (void)id;
    (void)spec;
}
const struct ui_cart_spec_t *cart_registry_get_ui_spec(cart_id_t id) {
    (void)id;
    return NULL;
}
const struct ui_cart_spec_t *cart_registry_switch(cart_id_t id) {
    (void)id;
    return NULL;
}
cart_id_t cart_registry_get_active_id(void) { return CART1; }
bool cart_registry_is_present(cart_id_t id) {
    (void)id;
    return false;
}
void cart_registry_set_uid(cart_id_t id, uint32_t uid) {
    (void)id;
    (void)uid;
}
uint32_t cart_registry_get_uid(cart_id_t id) {
    (void)id;
    return 0U;
}
bool cart_registry_find_by_uid(uint32_t uid, cart_id_t *out) {
    (void)uid;
    if (out != NULL) {
        *out = CART1;
    }
    return false;
}

seq_cold_view_t seq_runtime_cold_view(seq_cold_domain_t domain) {
    (void)domain;
    seq_cold_view_t view = {0};
    return view;
}
