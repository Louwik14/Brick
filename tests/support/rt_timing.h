#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void   rt_tim_reset(void);
void   rt_tim_tick_begin(void);
void   rt_tim_tick_end(void);
void   rt_tim_report(void);
double rt_tim_p99_ns(void);
#ifdef __cplusplus
}
#endif
