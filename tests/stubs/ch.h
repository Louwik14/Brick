#ifndef BRICK_TEST_CH_STUB_H
#define BRICK_TEST_CH_STUB_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t systime_t;
typedef int32_t msg_t;
typedef uint32_t tprio_t;
typedef struct { int dummy; } semaphore_t;
typedef struct { int dummy; } mutex_t;
typedef struct { bool taken; } binary_semaphore_t;
typedef struct { int dummy; } thread_t;
typedef void BaseSequentialStream;
typedef void (*tfunc_t)(void *arg);

#define MSG_OK      ((msg_t)0)
#define MSG_RESET   ((msg_t)-1)

systime_t chVTGetSystemTimeX(void);
systime_t chVTGetSystemTime(void);
void chThdSleepMilliseconds(uint32_t ms);
void chSysLock(void);
void chSysUnlock(void);
void chSysLockFromISR(void);
void chSysUnlockFromISR(void);

#define TIME_IMMEDIATE ((systime_t)0)
#define TIME_INFINITE  ((systime_t)-1)
#define TIME_MS2I(ms) ((systime_t)(ms))

#define chDbgCheck(cond) do { (void)(cond); } while (0)
#define chDbgAssert(cond, msg) do { if (!(cond)) { (void)(msg); } } while (0)
#define chDbgCheckClassI() do {} while (0)

#define THD_WORKING_AREA(name, size) uint8_t name[size]
#define THD_FUNCTION(name, arg) void name(void *arg)
#define NORMALPRIO 0
#define CH_CFG_USE_REGISTRY 0

void chMtxObjectInit(mutex_t *mtx);
void chMtxLock(mutex_t *mtx);
void chMtxUnlock(mutex_t *mtx);
void chBSemObjectInit(binary_semaphore_t *sem, bool taken);
msg_t chBSemWaitTimeout(binary_semaphore_t *sem, systime_t timeout);
void chBSemSignal(binary_semaphore_t *sem);
thread_t *chThdCreateStatic(void *wa, size_t size, tprio_t prio, tfunc_t func, void *arg);
void chThdWait(thread_t *thread);
void chRegSetThreadName(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_TEST_CH_STUB_H */
