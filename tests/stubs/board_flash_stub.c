#include <string.h>
#include "board/board_flash.h"

bool board_flash_init(void) { return true; }
bool board_flash_is_ready(void) { return true; }
uint32_t board_flash_get_capacity(void) { return BOARD_FLASH_CAPACITY_BYTES; }
uint32_t board_flash_get_sector_size(void) { return BOARD_FLASH_SECTOR_SIZE; }

bool board_flash_read(uint32_t address, void *buffer, size_t length)
{
    (void)address;
    if ((buffer != NULL) && (length > 0U)) {
        memset(buffer, 0, length);
    }
    return true;
}

bool board_flash_write(uint32_t address, const void *data, size_t length)
{
    (void)address;
    (void)data;
    (void)length;
    return true;
}

bool board_flash_erase(uint32_t address, size_t length)
{
    (void)address;
    (void)length;
    return true;
}

bool board_flash_erase_sector(uint32_t address)
{
    (void)address;
    return true;
}
