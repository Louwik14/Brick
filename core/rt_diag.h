/**
 * @file rt_diag.h
 * @brief Outils de diagnostic temps réel (threads, stacks, stats UI/LED).
 */
#ifndef BRICK_CORE_RT_DIAG_H
#define BRICK_CORE_RT_DIAG_H

#include "brick_config.h"

#if DEBUG_ENABLE

#include "ch.h"

void rt_dump_threads(BaseSequentialStream *stream);
void rt_diag_dump_stats(BaseSequentialStream *stream);
void rt_diag_record_panic_reason(const char *reason);
const char *rt_diag_get_last_panic_reason(void);

#else /* DEBUG_ENABLE */

#define rt_dump_threads(stream)           ((void)(stream))
#define rt_diag_dump_stats(stream)        ((void)(stream))
#define rt_diag_record_panic_reason(msg)  ((void)(msg))
#define rt_diag_get_last_panic_reason()   ("disabled")

#endif /* DEBUG_ENABLE */

#endif /* BRICK_CORE_RT_DIAG_H */
