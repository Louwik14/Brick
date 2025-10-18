/**
 * @file rt_diag.c
 * @brief Implémentation des diagnostics temps réel (threads, stats UI/LED).
 */

#include "brick_config.h"

#if DEBUG_ENABLE

#include "rt_diag.h"
#include "ch.h"
#include "chprintf.h"

#include "ui_led_backend.h"
#include "ui_task.h"

#ifndef CH_DBG_STACK_FILL_VALUE
#define CH_DBG_STACK_FILL_VALUE 0x55U
#endif

static CCM_DATA volatile const char *s_last_panic_reason = NULL;

typedef struct {
  size_t size;
  size_t used;
} rt_stack_info_t;

static void _rt_diag_compute_stack_usage(const thread_t *tp, rt_stack_info_t *info) {
  if ((tp == NULL) || (info == NULL)) {
    return;
  }

#if CH_DBG_FILL_THREADS
  const uint8_t *start = (const uint8_t *)tp->wend;
  const uint8_t *stop  = (const uint8_t *)tp->wabase;
  const size_t total   = (size_t)(stop - start);

  size_t idx;
  for (idx = 0U; idx < total; ++idx) {
    if (start[idx] != CH_DBG_STACK_FILL_VALUE) {
      break;
    }
  }

  info->size = total;
  info->used = (idx >= total) ? 0U : (total - idx);
#else
  (void)tp;
  info->size = 0U;
  info->used = 0U;
#endif
}

static const char *_rt_diag_state_name(thread_state_t st) {
  switch (st) {
    case CH_STATE_READY:      return "READY";
    case CH_STATE_CURRENT:    return "CURRENT";
    case CH_STATE_SLEEPING:   return "SLEEP";
    case CH_STATE_SUSPENDED:  return "SUSP";
    case CH_STATE_WTSEM:      return "WTSEM";
    case CH_STATE_WTMTX:      return "WTMTX";
    case CH_STATE_WTCOND:     return "WTCOND";
    case CH_STATE_WAITING:    return "WAIT";
    case CH_STATE_TERMINATED: return "DEAD";
    default:                  return "UNK";
  }
}

void rt_dump_threads(BaseSequentialStream *stream) {
#if CH_CFG_USE_REGISTRY
  if (stream == NULL) {
    return;
  }

  chSysLock();
  thread_t *tp = chRegFirstThread();
  while (tp != NULL) {
    thread_t *next = chRegNextThread(tp);
    thread_t *cur  = tp;
    chSysUnlock();

    rt_stack_info_t info = {0U, 0U};
    _rt_diag_compute_stack_usage(cur, &info);
    chprintf(stream,
             "[rt] th=%s state=%s prio=%u stack=%u/%u\r\n",
             cur->name ? cur->name : "(anon)",
             _rt_diag_state_name(cur->state),
             (unsigned)cur->prio,
             (unsigned)info.used,
             (unsigned)info.size);

    chSysLock();
    tp = next;
  }
  chSysUnlock();
#else
  (void)stream;
#endif
}

void rt_diag_dump_stats(BaseSequentialStream *stream) {
  if (stream == NULL) {
    return;
  }

  chprintf(stream,
           "[rt] LED mb: fail=%lu high=%lu/%u\r\n",
           (unsigned long)ui_led_backend_get_post_fail_count(),
           (unsigned long)ui_led_backend_get_high_watermark(),
           (unsigned)UI_LED_BACKEND_QUEUE_CAPACITY);

  chprintf(stream,
           "[rt] UI loop: window_max=%luus last_max=%luus\r\n",
           (unsigned long)ui_task_debug_get_loop_current_max_us(),
           (unsigned long)ui_task_debug_get_loop_last_max_us());

  const char *reason = rt_diag_get_last_panic_reason();
  chprintf(stream, "[rt] last panic: %s\r\n", reason ? reason : "(none)");

  rt_dump_threads(stream);
}

void rt_diag_record_panic_reason(const char *reason) {
  s_last_panic_reason = reason;
}

const char *rt_diag_get_last_panic_reason(void) {
  return s_last_panic_reason;
}

#endif /* DEBUG_ENABLE */
