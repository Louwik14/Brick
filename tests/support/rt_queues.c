#include "tests/support/rt_queues.h"

#if defined(SEQ_RT_QUEUE_MONITORING) && SEQ_RT_QUEUE_MONITORING

#include <stdio.h>

#include "core/seq/seq_engine.h"

#ifndef RQ_EVENT_CAPACITY
#define RQ_EVENT_CAPACITY SEQ_ENGINE_SCHEDULER_CAPACITY
#endif

#ifndef RQ_PLAYER_CAPACITY
#define RQ_PLAYER_CAPACITY SEQ_ENGINE_SCHEDULER_CAPACITY
#endif

static uint32_t s_event_depth = 0U;
static uint32_t s_event_hwm = 0U;
static uint32_t s_player_depth = 0U;
static uint32_t s_player_hwm = 0U;
static int s_event_underflow = 0;
static int s_event_overflow = 0;
static int s_player_underflow = 0;
static int s_player_overflow = 0;

void rq_reset(void) {
    s_event_depth = 0U;
    s_event_hwm = 0U;
    s_player_depth = 0U;
    s_player_hwm = 0U;
    s_event_underflow = 0;
    s_event_overflow = 0;
    s_player_underflow = 0;
    s_player_overflow = 0;
}

void rq_event_enq(void) {
    if (s_event_depth >= RQ_EVENT_CAPACITY) {
        s_event_overflow = 1;
        return;
    }

    ++s_event_depth;
    if (s_event_depth > s_event_hwm) {
        s_event_hwm = s_event_depth;
    }
}

void rq_event_deq(void) {
    if (s_event_depth == 0U) {
        s_event_underflow = 1;
        return;
    }

    --s_event_depth;
}

void rq_player_enq(void) {
    if (s_player_depth >= RQ_PLAYER_CAPACITY) {
        s_player_overflow = 1;
        return;
    }

    ++s_player_depth;
    if (s_player_depth > s_player_hwm) {
        s_player_hwm = s_player_depth;
    }
}

void rq_player_deq(void) {
    if (s_player_depth == 0U) {
        s_player_underflow = 1;
        return;
    }

    --s_player_depth;
}

uint32_t rq_event_high_watermark(void) {
    return s_event_hwm;
}

uint32_t rq_player_high_watermark(void) {
    return s_player_hwm;
}

int rq_any_underflow_or_overflow(void) {
    return (s_event_underflow || s_event_overflow || s_player_underflow || s_player_overflow) ? 1 : 0;
}

void rq_report(void) {
    printf("rt_queues: event_hwm=%u player_hwm=%u event_underflow=%d event_overflow=%d player_underflow=%d player_overflow=%d\n",
           (unsigned)s_event_hwm,
           (unsigned)s_player_hwm,
           s_event_underflow,
           s_event_overflow,
           s_player_underflow,
           s_player_overflow);
}

#endif
