#ifndef RUNTIME_TRACE_H
#define RUNTIME_TRACE_H

#include "ch.h"
#include "hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  RUNTIME_STAGE_RESET = 0,
  RUNTIME_STAGE_BEFORE_HAL,
  RUNTIME_STAGE_AFTER_HAL,
  RUNTIME_STAGE_AFTER_SYS,
  RUNTIME_STAGE_BEFORE_USB,
  RUNTIME_STAGE_AFTER_USB,
  RUNTIME_STAGE_AFTER_MIDI,
  RUNTIME_STAGE_AFTER_DRIVERS,
  RUNTIME_STAGE_AFTER_UI_INIT,
  RUNTIME_STAGE_AFTER_LED_BACKEND,
  RUNTIME_STAGE_AFTER_UI_THREAD,
  RUNTIME_STAGE_MAIN_LOOP,
  RUNTIME_STAGE_COUNT
} runtime_boot_stage_t;

typedef struct {
  uint32_t                last_stage;
  systime_t               stage_stamp[RUNTIME_STAGE_COUNT];
  uint32_t                pll_m;
  uint32_t                pll_n;
  uint32_t                pll_p;
  uint32_t                pll_q;
  uint32_t                hse_hz;
  uint32_t                pll_input_hz;
  uint32_t                pll_vco_hz;
  uint32_t                sysclk_hz;
  uint32_t                hclk_hz;
  uint32_t                pclk1_hz;
  uint32_t                pclk2_hz;
  uint32_t                usb_hz;
} runtime_boot_diagnostics_t;

void runtime_trace_pre_init(void);
void runtime_trace_on_hal_ready(void);
void runtime_trace_mark_kernel_ready(void);
void runtime_trace_stage(runtime_boot_stage_t stage);
const runtime_boot_diagnostics_t *runtime_trace_get_boot_diag(void);

#ifdef __cplusplus
}
#endif

#endif /* RUNTIME_TRACE_H */
