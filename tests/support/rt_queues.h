#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#if defined(SEQ_RT_QUEUE_MONITORING) && SEQ_RT_QUEUE_MONITORING
void rq_reset(void);
void rq_event_enq(void);
void rq_event_deq(void);
void rq_player_enq(void);
void rq_player_deq(void);
uint32_t rq_event_high_watermark(void);
uint32_t rq_player_high_watermark(void);
int rq_any_underflow_or_overflow(void);
void rq_report(void);
#else
static inline void rq_reset(void) {}
static inline void rq_event_enq(void) {}
static inline void rq_event_deq(void) {}
static inline void rq_player_enq(void) {}
static inline void rq_player_deq(void) {}
static inline uint32_t rq_event_high_watermark(void) { return 0U; }
static inline uint32_t rq_player_high_watermark(void) { return 0U; }
static inline int rq_any_underflow_or_overflow(void) { return 0; }
static inline void rq_report(void) {}
#endif
#ifdef __cplusplus
}
#endif
