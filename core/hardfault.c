/**
 * @file hardfault.c
 * @brief Gestionnaire HardFault verbeux pour le debug temps réel.
 */

#include "brick_config.h"

#if DEBUG_ENABLE

#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "rt_diag.h"
#include "ui_led_backend.h"
#include "ui_task.h"

extern SerialDriver SD2;

static void _hardfault_dump(uint32_t *sp, uint32_t lr) {
  BaseSequentialStream *stream = (BaseSequentialStream *)&SD2;
  rt_diag_record_panic_reason("HardFault");
  chprintf(stream, "\r\n[hardfault] R0=%08lx R1=%08lx R2=%08lx R3=%08lx\r\n",
           sp[0], sp[1], sp[2], sp[3]);
  chprintf(stream, "[hardfault] R12=%08lx LR=%08lx PC=%08lx PSR=%08lx\r\n",
           sp[4], lr, sp[6], sp[7]);
  chprintf(stream, "[hardfault] LED queue: fail=%lu high=%lu/%u\r\n",
           (unsigned long)ui_led_backend_get_post_fail_count(),
           (unsigned long)ui_led_backend_get_high_watermark(),
           (unsigned)UI_LED_BACKEND_QUEUE_CAPACITY);
  chprintf(stream, "[hardfault] UI loop max: cur=%luus last=%luus\r\n",
           (unsigned long)ui_task_debug_get_loop_current_max_us(),
           (unsigned long)ui_task_debug_get_loop_last_max_us());
  while (true) {
    __asm volatile ("bkpt #0");
  }
}

__attribute__((naked)) void HardFault_Handler(void) {
  __asm volatile (
      "tst lr, #4      \n"
      "ite eq          \n"
      "mrseq r0, msp   \n"
      "mrsne r0, psp   \n"
      "mov r1, lr      \n"
      "b _hardfault_c  \n");
}

void _hardfault_c(uint32_t *sp, uint32_t lr) {
  (void)lr;
  _hardfault_dump(sp, lr);
}

#endif /* DEBUG_ENABLE */
