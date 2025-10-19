#include "runtime_trace.h"

#include <string.h>

#define TRACE_LINE0   LINE_LED1
#define TRACE_LINE1   LINE_LED2
#define TRACE_LINE2   LINE_LED3

static runtime_boot_diagnostics_t s_diag;
static bool s_trace_prepared = false;
static bool s_gpio_ready = false;
static bool s_kernel_ready = false;

static uint32_t decode_hpre_div(uint32_t hpre_bits) {
  switch (hpre_bits) {
    case 0U:  return 1U;
    case 8U:  return 2U;
    case 9U:  return 4U;
    case 10U: return 8U;
    case 11U: return 16U;
    case 12U: return 64U;
    case 13U: return 128U;
    case 14U: return 256U;
    case 15U: return 512U;
    default:  return 1U;
  }
}

static uint32_t decode_ppre_div(uint32_t ppre_bits) {
  switch (ppre_bits) {
    case 0U: return 1U;
    case 4U: return 2U;
    case 5U: return 4U;
    case 6U: return 8U;
    case 7U: return 16U;
    default: return 1U;
  }
}

static void runtime_trace_update_leds(uint32_t stage) {
  palWriteLine(TRACE_LINE0, (stage & 0x1U) ? PAL_HIGH : PAL_LOW);
  palWriteLine(TRACE_LINE1, (stage & 0x2U) ? PAL_HIGH : PAL_LOW);
  palWriteLine(TRACE_LINE2, (stage & 0x4U) ? PAL_HIGH : PAL_LOW);
}

static void runtime_trace_capture_clocks(void) {
  uint32_t cfgr    = RCC->CFGR;
  uint32_t pllcfgr = RCC->PLLCFGR;

  uint32_t pll_m = pllcfgr & RCC_PLLCFGR_PLLM;
  uint32_t pll_n = (pllcfgr & RCC_PLLCFGR_PLLN) >> RCC_PLLCFGR_PLLN_Pos;
  uint32_t pll_p_bits = (pllcfgr & RCC_PLLCFGR_PLLP) >> RCC_PLLCFGR_PLLP_Pos;
  uint32_t pll_q = (pllcfgr & RCC_PLLCFGR_PLLQ) >> RCC_PLLCFGR_PLLQ_Pos;

  uint32_t pll_p = ((pll_p_bits + 1U) * 2U);
  if (pll_p == 0U) {
    pll_p = 2U;
  }

  s_diag.pll_m = pll_m;
  s_diag.pll_n = pll_n;
  s_diag.pll_p = pll_p;
  s_diag.pll_q = pll_q;

  const uint32_t hse_hz = STM32_HSECLK;
  s_diag.hse_hz = hse_hz;

  uint32_t pll_input_hz = 0U;
  uint32_t pll_vco_hz   = 0U;
  uint32_t sysclk_hz    = 0U;
  uint32_t usb_hz       = 0U;

  if ((pll_m != 0U) && (pll_n != 0U)) {
    pll_input_hz = hse_hz / pll_m;
    pll_vco_hz   = pll_input_hz * pll_n;
    if (pll_p != 0U) {
      sysclk_hz = pll_vco_hz / pll_p;
    }
    if (pll_q != 0U) {
      usb_hz = pll_vco_hz / pll_q;
    }
  }

  s_diag.pll_input_hz = pll_input_hz;
  s_diag.pll_vco_hz   = pll_vco_hz;
  s_diag.sysclk_hz    = sysclk_hz;
  s_diag.usb_hz       = usb_hz;

  uint32_t hpre_bits  = (cfgr & RCC_CFGR_HPRE) >> RCC_CFGR_HPRE_Pos;
  uint32_t ppre1_bits = (cfgr & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;
  uint32_t ppre2_bits = (cfgr & RCC_CFGR_PPRE2) >> RCC_CFGR_PPRE2_Pos;

  uint32_t hpre_div  = decode_hpre_div(hpre_bits);
  uint32_t ppre1_div = decode_ppre_div(ppre1_bits);
  uint32_t ppre2_div = decode_ppre_div(ppre2_bits);

  s_diag.hclk_hz  = (hpre_div  != 0U) ? (sysclk_hz / hpre_div)  : 0U;
  s_diag.pclk1_hz = (ppre1_div != 0U) ? (s_diag.hclk_hz / ppre1_div) : 0U;
  s_diag.pclk2_hz = (ppre2_div != 0U) ? (s_diag.hclk_hz / ppre2_div) : 0U;
}

void runtime_trace_pre_init(void) {
  memset(&s_diag, 0, sizeof(s_diag));
  s_diag.last_stage = RUNTIME_STAGE_RESET;
  s_trace_prepared = true;
  s_gpio_ready = false;
  s_kernel_ready = false;
}

void runtime_trace_on_hal_ready(void) {
  if (!s_trace_prepared) {
    runtime_trace_pre_init();
  }

  palSetLineMode(TRACE_LINE0, PAL_MODE_OUTPUT_PUSHPULL);
  palSetLineMode(TRACE_LINE1, PAL_MODE_OUTPUT_PUSHPULL);
  palSetLineMode(TRACE_LINE2, PAL_MODE_OUTPUT_PUSHPULL);
  s_gpio_ready = true;

  runtime_trace_update_leds(s_diag.last_stage);
  runtime_trace_capture_clocks();
}

void runtime_trace_mark_kernel_ready(void) {
  s_kernel_ready = true;
}

void runtime_trace_stage(runtime_boot_stage_t stage) {
  if (!s_trace_prepared) {
    runtime_trace_pre_init();
  }

  s_diag.last_stage = (uint32_t)stage;

  if (stage < RUNTIME_STAGE_COUNT) {
    if (s_kernel_ready) {
      s_diag.stage_stamp[stage] = chVTGetSystemTimeX();
    } else {
      s_diag.stage_stamp[stage] = 0U;
    }
  }

  if (s_gpio_ready) {
    runtime_trace_update_leds((uint32_t)stage);
  }
}

const runtime_boot_diagnostics_t *runtime_trace_get_boot_diag(void) {
  return &s_diag;
}
