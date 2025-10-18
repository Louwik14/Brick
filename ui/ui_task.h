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

#ifdef __cplusplus
}
#endif

#endif /* BRICK_UI_UI_TASK_H */
