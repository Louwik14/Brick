#ifndef BRICK_TEST_CH_STUB_H
#define BRICK_TEST_CH_STUB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t systime_t;
typedef struct { int dummy; } semaphore_t;
typedef struct { int dummy; } mutex_t;
typedef void thread_t;
typedef void BaseSequentialStream;
typedef int msg_t;
typedef struct {
    bool signaled;
} binary_semaphore_t;

#define MSG_OK 0
#define MSG_RESET (-1)

systime_t chVTGetSystemTimeX(void);
systime_t chVTGetSystemTime(void);
void chThdSleepMilliseconds(uint32_t ms);
void chThdSleepMicroseconds(uint32_t us);
void chRegSetThreadName(const char *name);
thread_t *chThdCreateStatic(void *wa, size_t size, int prio, void (*func)(void *), void *arg);
msg_t chThdWait(thread_t *tp);
int chsnprintf(char *buf, size_t size, const char *fmt, ...);

#define NORMALPRIO 0
#define THD_WORKING_AREA(name, size) uint8_t name[(size)]
#define THD_FUNCTION(name, arg) void name(void *arg)

void chSysLock(void);
void chSysUnlock(void);
void chSysLockFromISR(void);
void chSysUnlockFromISR(void);

void chMtxObjectInit(mutex_t *mp);
void chMtxLock(mutex_t *mp);
void chMtxUnlock(mutex_t *mp);

void chBSemObjectInit(binary_semaphore_t *bsp, bool taken);
msg_t chBSemWaitTimeout(binary_semaphore_t *bsp, systime_t timeout);
void chBSemSignal(binary_semaphore_t *bsp);

#define TIME_MS2I(ms) ((systime_t)(ms))
#define TIME_INFINITE ((systime_t)(-1))

#define chDbgCheck(cond) do { (void)(cond); } while (0)
#define chDbgAssert(cond, msg) do { if (!(cond)) { (void)(msg); } } while (0)
#define chDbgCheckClassI() do {} while (0)

extern systime_t g_ch_stub_time;
void ch_stub_set_time(systime_t now);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_TEST_CH_STUB_H */
