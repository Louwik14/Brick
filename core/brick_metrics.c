#include "brick_metrics.h"

#include "ch.h"
#include "midi.h"
#include "cart_bus.h"
#include "drivers/drv_buttons.h"
#include "ui/ui_led_backend.h"

static size_t _thread_stack_size(thread_t *tp) {
  if (tp == NULL) {
    return 0U;
  }
  const uint8_t *base = (const uint8_t *)tp->wabase;
  const uint8_t *top = (const uint8_t *)tp;
  if (top <= base) {
    return 0U;
  }
  return (size_t)(top - base);
}

static size_t _thread_stack_used(thread_t *tp, size_t stack_size) {
  if ((tp == NULL) || (stack_size == 0U)) {
    return 0U;
  }
#if (CH_DBG_FILL_THREADS == TRUE)
  const uint8_t *start = (const uint8_t *)tp->wabase;
  const uint8_t *stack_top = (const uint8_t *)tp;
  const uint8_t *p = start;
  while ((p < stack_top) && (*p == (uint8_t)CH_DBG_STACK_FILL_VALUE)) {
    ++p;
  }
  size_t unused = (size_t)(p - start);
  if (unused > stack_size) {
    unused = stack_size;
  }
  return stack_size - unused;
#else
  (void)tp;
  (void)stack_size;
  return 0U;
#endif
}

size_t brick_metrics_collect_stacks(brick_stack_metric_t *out, size_t capacity) {
  if ((out == NULL) || (capacity == 0U)) {
    return 0U;
  }

  size_t count = 0U;
  thread_t *tp = chRegFirstThread();
  if (tp == NULL) {
    return 0U;
  }

  do {
    if (count < capacity) {
      const char *name = (tp->name != NULL) ? tp->name : "<unnamed>";
      const size_t stack_size = _thread_stack_size(tp);
      const size_t used = _thread_stack_used(tp, stack_size);
      out[count].name = name;
      out[count].stack_size_bytes = stack_size;
      out[count].stack_used_bytes = used;
      out[count].stack_free_bytes = (stack_size > used) ? (stack_size - used) : 0U;
      ++count;
    }
    tp = chRegNextThread(tp);
  } while (tp != NULL);

  return count;
}

size_t brick_metrics_collect_queues(brick_queue_metric_t *out, size_t capacity) {
  if ((out == NULL) || (capacity == 0U)) {
    return 0U;
  }

  size_t count = 0U;
#if defined(BRICK_ENABLE_INSTRUMENTATION)
  if (count < capacity) {
    out[count].name = "MIDI USB";
    out[count].capacity = MIDI_USB_QUEUE_LEN;
    out[count].high_water = midi_usb_queue_high_watermark();
    out[count].current_fill = midi_usb_queue_fill_level();
    out[count].drop_count = midi_tx_stats.tx_mb_drops;
    ++count;
  }

  static const char *kCartNames[CART_COUNT] = {
    "Cart1 TX", "Cart2 TX", "Cart3 TX", "Cart4 TX"
  };
  for (cart_id_t id = CART1; id < CART_COUNT && count < capacity; ++id) {
    out[count].name = kCartNames[id];
    out[count].capacity = CART_QUEUE_LEN;
    out[count].high_water = cart_bus_get_mailbox_high_water(id);
    out[count].current_fill = cart_bus_get_mailbox_fill(id);
    out[count].drop_count = cart_stats[id].mb_full;
    ++count;
  }

  if (count < capacity) {
    out[count].name = "Buttons";
    out[count].capacity = DRV_BUTTONS_QUEUE_LEN;
    out[count].high_water = drv_buttons_queue_high_water();
    out[count].current_fill = drv_buttons_queue_fill();
    out[count].drop_count = drv_buttons_queue_drop_count();
    ++count;
  }

  if (count < capacity) {
    out[count].name = "LED backend";
    out[count].capacity = UI_LED_BACKEND_QUEUE_CAPACITY;
    out[count].high_water = ui_led_backend_queue_high_water();
    out[count].current_fill = ui_led_backend_queue_fill();
    out[count].drop_count = ui_led_backend_queue_drop_count();
    ++count;
  }
#else
  (void)out;
  (void)capacity;
#endif
  return count;
}

void brick_metrics_reset_queue_counters(void) {
#if defined(BRICK_ENABLE_INSTRUMENTATION)
  midi_usb_queue_reset_stats();
  midi_stats_reset();
  cart_bus_reset_mailbox_stats();
  drv_buttons_stats_reset();
  ui_led_backend_queue_reset_stats();
#endif
}

bool brick_metrics_get_led_backend_timing(brick_led_timing_metric_t *out) {
#if defined(BRICK_ENABLE_INSTRUMENTATION)
  if (out == NULL) {
    return false;
  }

  out->refresh_last_ticks = ui_led_backend_last_refresh_ticks();
  out->refresh_max_ticks = ui_led_backend_max_refresh_ticks();
  out->render_last_ticks = ui_led_backend_last_render_ticks();
  out->render_max_ticks = ui_led_backend_max_render_ticks();
  out->tick_frequency_hz = chSysGetRealtimeCounterFrequency();
  return true;
#else
  (void)out;
  return false;
#endif
}
