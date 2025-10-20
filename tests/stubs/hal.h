/*
 * @file tests/stubs/hal.h
 * @brief Minimal HAL stub to allow host / audit builds without ChibiOS.
 */

#ifndef BRICK_TEST_HAL_STUB_H
#define BRICK_TEST_HAL_STUB_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    void     *ssport;
    uint32_t sspad;
    uint32_t cr1;
    uint32_t cr2;
} SPIConfig;

typedef struct {
    int dummy;
} SPIDriver;

typedef struct {
    volatile uint32_t BSRR;
} GPIOStubPort;

extern SPIDriver SPID1;
extern GPIOStubPort GPIOB_stub;
extern GPIOStubPort GPIOD_stub;

#define SPI_CR1_BR_2 0U

#define GPIOB (&GPIOB_stub)
#define GPIOD (&GPIOD_stub)

#define PAL_MODE_OUTPUT_PUSHPULL 0U

static inline void spiStart(SPIDriver *driver, const SPIConfig *config) {
    (void)driver;
    (void)config;
}

static inline void spiSelect(SPIDriver *driver) {
    (void)driver;
}

static inline void spiUnselect(SPIDriver *driver) {
    (void)driver;
}

static inline void spiSend(SPIDriver *driver, size_t n, const void *txbuf) {
    (void)driver;
    (void)n;
    (void)txbuf;
}

static inline void palSetPadMode(GPIOStubPort *port, uint32_t pad, uint32_t mode) {
    (void)port;
    (void)pad;
    (void)mode;
}

static inline void palSetPad(GPIOStubPort *port, uint32_t pad) {
    (void)port;
    (void)pad;
}

static inline void palClearPad(GPIOStubPort *port, uint32_t pad) {
    (void)port;
    (void)pad;
}

#endif /* BRICK_TEST_HAL_STUB_H */
