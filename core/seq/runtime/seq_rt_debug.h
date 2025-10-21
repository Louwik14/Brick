#pragma once
#include "core/seq/seq_config.h"
#if defined(SEQ_RT_DEBUG) && SEQ_RT_DEBUG
extern volatile unsigned g_rt_tick_events_max;
extern volatile unsigned g_rt_event_queue_hwm;
#endif
