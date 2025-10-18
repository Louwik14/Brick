/**
 * @file ui_task.h
 * @brief Lancement du thread UI (lecture entrées + rendu).
 */
#ifndef BRICK_UI_UI_TASK_H
#define BRICK_UI_UI_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

void ui_task_start(void);
bool ui_task_is_running(void);

#if DEBUG_ENABLE
uint32_t ui_task_debug_get_loop_current_max_us(void);
uint32_t ui_task_debug_get_loop_last_max_us(void);
#else
static inline uint32_t ui_task_debug_get_loop_current_max_us(void) { return 0U; }
static inline uint32_t ui_task_debug_get_loop_last_max_us(void) { return 0U; }
#endif

#ifdef __cplusplus
}
#endif

#endif /* BRICK_UI_UI_TASK_H */
