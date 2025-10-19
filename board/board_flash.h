#ifndef BRICK_BOARD_BOARD_FLASH_H_
#define BRICK_BOARD_BOARD_FLASH_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BOARD_FLASH_CAPACITY_BYTES
#define BOARD_FLASH_CAPACITY_BYTES (16U * 1024U * 1024U)
#endif

#ifndef BOARD_FLASH_SECTOR_SIZE
#define BOARD_FLASH_SECTOR_SIZE 4096U
#endif

/**
 * @brief Maximum size, in bytes, accepted by the RAM shadow simulator.
 *
 * The embedded target cannot reserve tens of megabytes for a mirror of the
 * external flash.  When @ref BOARD_FLASH_CAPACITY_BYTES exceeds this value the
 * simulator backend is disabled and @ref board_flash_init() falls back to a
 * "not ready" state unless a hardware backend is provided. A value of zero
 * disables the simulator unconditionally.
 */
#ifndef BOARD_FLASH_SIMULATOR_MAX_CAPACITY
#define BOARD_FLASH_SIMULATOR_MAX_CAPACITY  (0U)
#endif

bool board_flash_init(void);
bool board_flash_is_ready(void);
uint32_t board_flash_get_capacity(void);
uint32_t board_flash_get_sector_size(void);
bool board_flash_read(uint32_t address, void *buffer, size_t length);
bool board_flash_write(uint32_t address, const void *data, size_t length);
bool board_flash_erase(uint32_t address, size_t length);
bool board_flash_erase_sector(uint32_t address);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_BOARD_BOARD_FLASH_H_ */
