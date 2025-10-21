#include "core/seq/runtime/seq_rt_debug.h"

#if defined(SEQ_RT_DEBUG) && SEQ_RT_DEBUG

#include <stddef.h>

volatile unsigned g_rt_tick_events_max;
volatile unsigned g_rt_event_queue_hwm;

__attribute__((weak)) void seq_rt_debug_uart_write(const char *data, size_t len) {
    (void)data;
    (void)len;
}

static size_t seq_rt_debug_format_uint(unsigned value, char *buffer) {
    char tmp[10];
    size_t digits = 0U;
    do {
        tmp[digits++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while (value != 0U);

    for (size_t i = 0U; i < digits; ++i) {
        buffer[i] = tmp[digits - 1U - i];
    }
    return digits;
}

void seq_rt_debug_report_uart_once_per_sec(void) {
    char line[48];
    size_t pos = 0U;
    const char prefix[] = "rt: ev_max=";
    const char middle[] = " q_hwm=";

    for (size_t i = 0U; i < sizeof(prefix) - 1U; ++i) {
        line[pos++] = prefix[i];
    }
    pos += seq_rt_debug_format_uint(g_rt_tick_events_max, &line[pos]);

    for (size_t i = 0U; i < sizeof(middle) - 1U; ++i) {
        line[pos++] = middle[i];
    }
    pos += seq_rt_debug_format_uint(g_rt_event_queue_hwm, &line[pos]);

    line[pos++] = '\n';
    seq_rt_debug_uart_write(line, pos);
}

#endif
