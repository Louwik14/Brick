#include <stdarg.h>
#include <stdio.h>

#include "ch.h"

systime_t g_ch_stub_time;

systime_t chVTGetSystemTimeX(void) {
    return g_ch_stub_time;
}

systime_t chVTGetSystemTime(void) {
    return g_ch_stub_time;
}

void chThdSleepMilliseconds(uint32_t ms) {
    g_ch_stub_time += (systime_t)ms;
}

void chThdSleepMicroseconds(uint32_t us) {
    g_ch_stub_time += (systime_t)(us / 1000U);
}

void chRegSetThreadName(const char *name) {
    (void)name;
}

thread_t *chThdCreateStatic(void *wa, size_t size, int prio, void (*func)(void *), void *arg) {
    (void)wa;
    (void)size;
    (void)prio;
    (void)func;
    (void)arg;
    return (thread_t *)1;
}

msg_t chThdWait(thread_t *tp) {
    (void)tp;
    return MSG_OK;
}

int chsnprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(buf, size, fmt, args);
    va_end(args);
    return written;
}

void chSysLock(void) {}
void chSysUnlock(void) {}
void chSysLockFromISR(void) {}
void chSysUnlockFromISR(void) {}

void chMtxObjectInit(mutex_t *mp) {
    (void)mp;
}

void chMtxLock(mutex_t *mp) {
    (void)mp;
}

void chMtxUnlock(mutex_t *mp) {
    (void)mp;
}

void chBSemObjectInit(binary_semaphore_t *bsp, bool taken) {
    if (bsp != NULL) {
        bsp->signaled = !taken;
    }
}

msg_t chBSemWaitTimeout(binary_semaphore_t *bsp, systime_t timeout) {
    (void)timeout;
    if (bsp != NULL) {
        bsp->signaled = false;
    }
    return MSG_OK;
}

void chBSemSignal(binary_semaphore_t *bsp) {
    if (bsp != NULL) {
        bsp->signaled = true;
    }
}

void ch_stub_set_time(systime_t now) {
    g_ch_stub_time = now;
}
