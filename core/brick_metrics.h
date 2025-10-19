#ifndef BRICK_METRICS_H
#define BRICK_METRICS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  const char *name;
  size_t stack_size_bytes;
  size_t stack_used_bytes;
  size_t stack_free_bytes;
} brick_stack_metric_t;

typedef struct {
  const char *name;
  uint16_t capacity;
  uint16_t high_water;
  uint16_t current_fill;
  uint32_t drop_count;
} brick_queue_metric_t;

typedef struct {
  uint32_t refresh_last_ticks;
  uint32_t refresh_max_ticks;
  uint32_t render_last_ticks;
  uint32_t render_max_ticks;
  uint32_t tick_frequency_hz;
} brick_led_timing_metric_t;

size_t brick_metrics_collect_stacks(brick_stack_metric_t *out, size_t capacity);
size_t brick_metrics_collect_queues(brick_queue_metric_t *out, size_t capacity);
void brick_metrics_reset_queue_counters(void);
bool brick_metrics_get_led_backend_timing(brick_led_timing_metric_t *out);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_METRICS_H */
