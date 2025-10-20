#ifndef BRICK_TEST_CH_STUB_H
#define BRICK_TEST_CH_STUB_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t systime_t;
typedef struct { int dummy; } semaphore_t;
typedef struct { int dummy; } mutex_t;
typedef void thread_t;
typedef void BaseSequentialStream;

systime_t chVTGetSystemTimeX(void);
systime_t chVTGetSystemTime(void);
void chThdSleepMilliseconds(uint32_t ms);
void chThdSleepMicroseconds(uint32_t us);
void chRegSetThreadName(const char *name);
void chThdCreateStatic(void *wa, size_t size, int prio, void (*func)(void *), void *arg);
int chsnprintf(char *buf, size_t size, const char *fmt, ...);

#define NORMALPRIO 0
#define THD_WORKING_AREA(name, size) uint8_t name[(size)]
#define THD_FUNCTION(name, arg) void name(void *arg)
void chSysLock(void);
void chSysUnlock(void);
void chSysLockFromISR(void);
void chSysUnlockFromISR(void);

#define TIME_MS2I(ms) ((systime_t)(ms))

#define chDbgCheck(cond) do { (void)(cond); } while (0)
#define chDbgAssert(cond, msg) do { if (!(cond)) { (void)(msg); } } while (0)
#define chDbgCheckClassI() do {} while (0)

#ifdef __cplusplus
}
#endif

#endif /* BRICK_TEST_CH_STUB_H */
