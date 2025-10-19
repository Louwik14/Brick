#ifndef BRICK_METRICS_H
#define BRICK_METRICS_H

#include <stddef.h>
#include <stdint.h>

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

size_t brick_metrics_collect_stacks(brick_stack_metric_t *out, size_t capacity);
size_t brick_metrics_collect_queues(brick_queue_metric_t *out, size_t capacity);
void brick_metrics_reset_queue_counters(void);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_METRICS_H */
