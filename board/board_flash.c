/**
 * @file board_flash.c
 * @brief External flash access helpers with simulator fallback.
 */

#include "board_flash.h"

#include <stdlib.h>
#include <string.h>

#ifndef BOARD_FLASH_SIMULATOR_FILL
#define BOARD_FLASH_SIMULATOR_FILL 0xFFU
#endif

#ifndef BOARD_FLASH_MAX_ADDRESS
#define BOARD_FLASH_MAX_ADDRESS (BOARD_FLASH_CAPACITY_BYTES)
#endif

static bool s_initialized;
static bool s_use_hw;
static uint8_t *s_shadow;

__attribute__((weak)) bool board_flash_hw_init(void) {
    return false;
}

__attribute__((weak)) bool board_flash_hw_read(uint32_t address, void *buffer, size_t length) {
    (void)address;
    (void)buffer;
    (void)length;
    return false;
}

__attribute__((weak)) bool board_flash_hw_write(uint32_t address, const void *data, size_t length) {
    (void)address;
    (void)data;
    (void)length;
    return false;
}

__attribute__((weak)) bool board_flash_hw_erase_sector(uint32_t address) {
    (void)address;
    return false;
}

static bool shadow_alloc(void) {
    if (s_shadow != NULL) {
        return true;
    }
    s_shadow = (uint8_t *)malloc(BOARD_FLASH_CAPACITY_BYTES);
    if (s_shadow == NULL) {
        return false;
    }
    memset(s_shadow, BOARD_FLASH_SIMULATOR_FILL, BOARD_FLASH_CAPACITY_BYTES);
    return true;
}

static bool check_bounds(uint32_t address, size_t length) {
    if (length == 0U) {
        return true;
    }
    if (address >= BOARD_FLASH_MAX_ADDRESS) {
        return false;
    }
    const uint64_t end = (uint64_t)address + (uint64_t)length;
    return end <= BOARD_FLASH_MAX_ADDRESS;
}

bool board_flash_init(void) {
    if (s_initialized) {
        return true;
    }

    if (board_flash_hw_init()) {
        s_use_hw = true;
        s_initialized = true;
        return true;
    }

    if (!shadow_alloc()) {
        return false;
    }

    s_use_hw = false;
    s_initialized = true;
    return true;
}

bool board_flash_is_ready(void) {
    return s_initialized;
}

uint32_t board_flash_get_capacity(void) {
    return BOARD_FLASH_CAPACITY_BYTES;
}

uint32_t board_flash_get_sector_size(void) {
    return BOARD_FLASH_SECTOR_SIZE;
}

bool board_flash_read(uint32_t address, void *buffer, size_t length) {
    if (!board_flash_is_ready() || (buffer == NULL)) {
        return false;
    }
    if (!check_bounds(address, length)) {
        return false;
    }
    if (length == 0U) {
        return true;
    }

    if (s_use_hw) {
        return board_flash_hw_read(address, buffer, length);
    }

    memcpy(buffer, &s_shadow[address], length);
    return true;
}

static bool shadow_write(uint32_t address, const uint8_t *data, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        const uint8_t current = s_shadow[address + i];
        const uint8_t incoming = data[i];
        if ((uint8_t)(~current) & incoming) {
            return false;
        }
        s_shadow[address + i] = (uint8_t)(current & incoming);
    }
    return true;
}

bool board_flash_write(uint32_t address, const void *data, size_t length) {
    if (!board_flash_is_ready() || (data == NULL)) {
        return false;
    }
    if (!check_bounds(address, length)) {
        return false;
    }
    if (length == 0U) {
        return true;
    }

    if (s_use_hw) {
        return board_flash_hw_write(address, data, length);
    }

    return shadow_write(address, (const uint8_t *)data, length);
}

bool board_flash_erase_sector(uint32_t address) {
    if (!board_flash_is_ready()) {
        return false;
    }

    const uint32_t sector_size = board_flash_get_sector_size();
    const uint32_t aligned = address - (address % sector_size);
    if (!check_bounds(aligned, sector_size)) {
        return false;
    }

    if (s_use_hw) {
        return board_flash_hw_erase_sector(aligned);
    }

    memset(&s_shadow[aligned], BOARD_FLASH_SIMULATOR_FILL, sector_size);
    return true;
}

bool board_flash_erase(uint32_t address, size_t length) {
    if (!board_flash_is_ready()) {
        return false;
    }
    if (!check_bounds(address, length)) {
        return false;
    }
    if (length == 0U) {
        return true;
    }

    const uint32_t sector_size = board_flash_get_sector_size();
    uint32_t cursor = address - (address % sector_size);
    const uint32_t end = address + (uint32_t)length;

    while (cursor < end) {
        if (!board_flash_erase_sector(cursor)) {
            return false;
        }
        cursor += sector_size;
    }

    return true;
}
