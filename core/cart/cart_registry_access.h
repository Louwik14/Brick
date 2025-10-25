#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__)
bool cart_registry_cart_name(uint8_t cart_index, char *out, uint8_t out_len) __attribute__((weak));
#else
bool cart_registry_cart_name(uint8_t cart_index, char *out, uint8_t out_len);
#endif

#ifdef __cplusplus
}
#endif
