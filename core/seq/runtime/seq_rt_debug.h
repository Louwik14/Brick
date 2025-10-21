#pragma once
#include "core/seq/seq_config.h"
#if defined(SEQ_RT_DEBUG) && SEQ_RT_DEBUG
extern volatile unsigned g_rt_tick_events_max;
extern volatile unsigned g_rt_event_queue_hwm;
void seq_rt_debug_report_uart_once_per_sec(void);
#else
static inline void seq_rt_debug_report_uart_once_per_sec(void) {}
#endif
