/*
    ChibiOS - Copyright (C) 2006..2024 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

/**
 * @file    rt/templates/chconf.h
 * @brief   Configuration file (project-specific).
 *
 * @addtogroup config
 * @details Kernel related settings and hooks.
 * @{
 */

#ifndef CHCONF_H
#define CHCONF_H

#define _CHIBIOS_RT_CONF_
#define _CHIBIOS_RT_CONF_VER_8_0_

/* Option maintenue depuis ta config */
#if !defined(CH_CFG_HARDENING_LEVEL)
#define CH_CFG_HARDENING_LEVEL               0
#endif

/*===========================================================================*/
/**
 * @name System settings
 * @{
 */
/*===========================================================================*/

/**
 * @brief   Handling of instances (SMP mode).
 */
#if !defined(CH_CFG_SMP_MODE)
#define CH_CFG_SMP_MODE                      FALSE
#endif

/** @} */

/*===========================================================================*/
/**
 * @name System timers settings
 * @{
 */
/*===========================================================================*/

/**
 * @brief   System time counter resolution.
 * @note    Allowed values are 16, 32 or 64 bits.
 */
#if !defined(CH_CFG_ST_RESOLUTION)
#define CH_CFG_ST_RESOLUTION                 32
#endif

/**
 * @brief   System tick frequency.
 * @details Frequency of the system timer that drives the system ticks.
 */
#if !defined(CH_CFG_ST_FREQUENCY)
#define CH_CFG_ST_FREQUENCY                  10000
#endif

/**
 * @brief   Time intervals data size.
 * @note    Allowed values are 16, 32 or 64 bits.
 */
#if !defined(CH_CFG_INTERVALS_SIZE)
#define CH_CFG_INTERVALS_SIZE                32
#endif

/**
 * @brief   Time types data size.
 * @note    Allowed values are 16 or 32 bits.
 */
#if !defined(CH_CFG_TIME_TYPES_SIZE)
#define CH_CFG_TIME_TYPES_SIZE               32
#endif

/**
 * @brief   Time delta constant for the tick-less mode.
 * @note    If zero then classic periodic tick, otherwise tickless.
 */
#if !defined(CH_CFG_ST_TIMEDELTA)
#define CH_CFG_ST_TIMEDELTA                  2
#endif

/** @} */

/*===========================================================================*/
/**
 * @name Kernel parameters and options
 * @{
 */
/*===========================================================================*/

/**
 * @brief   Round robin interval.
 * @note    Must be 0 in tickless mode.
 */
#if !defined(CH_CFG_TIME_QUANTUM)
#define CH_CFG_TIME_QUANTUM                  0
#endif

/**
 * @brief   Idle thread automatic spawn suppression.
 */
#if !defined(CH_CFG_NO_IDLE_THREAD)
#define CH_CFG_NO_IDLE_THREAD                FALSE
#endif

/** @} */

/*===========================================================================*/
/**
 * @name Performance options
 * @{
 */
/*===========================================================================*/

/**
 * @brief   OS optimization for speed.
 */
#if !defined(CH_CFG_OPTIMIZE_SPEED)
#define CH_CFG_OPTIMIZE_SPEED                TRUE
#endif

/** @} */

/*===========================================================================*/
/**
 * @name Subsystem options
 * @{
 */
/*===========================================================================*/

#if !defined(CH_CFG_USE_TM)
#define CH_CFG_USE_TM                        TRUE
#endif

#if !defined(CH_CFG_USE_TIMESTAMP)
#define CH_CFG_USE_TIMESTAMP                 TRUE
#endif

#if !defined(CH_CFG_USE_REGISTRY)
#define CH_CFG_USE_REGISTRY                  TRUE
#endif

#if !defined(CH_CFG_USE_WAITEXIT)
#define CH_CFG_USE_WAITEXIT                  TRUE
#endif

#if !defined(CH_CFG_USE_SEMAPHORES)
#define CH_CFG_USE_SEMAPHORES                TRUE
#endif

#if !defined(CH_CFG_USE_SEMAPHORES_PRIORITY)
#define CH_CFG_USE_SEMAPHORES_PRIORITY       FALSE
#endif

#if !defined(CH_CFG_USE_MUTEXES)
#define CH_CFG_USE_MUTEXES                   TRUE
#endif

#if !defined(CH_CFG_USE_MUTEXES_RECURSIVE)
#define CH_CFG_USE_MUTEXES_RECURSIVE         FALSE
#endif

#if !defined(CH_CFG_USE_CONDVARS)
#define CH_CFG_USE_CONDVARS                  TRUE
#endif

#if !defined(CH_CFG_USE_CONDVARS_TIMEOUT)
#define CH_CFG_USE_CONDVARS_TIMEOUT          TRUE
#endif

#if !defined(CH_CFG_USE_EVENTS)
#define CH_CFG_USE_EVENTS                    TRUE
#endif

#if !defined(CH_CFG_USE_EVENTS_TIMEOUT)
#define CH_CFG_USE_EVENTS_TIMEOUT            TRUE
#endif

#if !defined(CH_CFG_USE_MESSAGES)
#define CH_CFG_USE_MESSAGES                  TRUE
#endif

#if !defined(CH_CFG_USE_MESSAGES_PRIORITY)
#define CH_CFG_USE_MESSAGES_PRIORITY         FALSE
#endif

#if !defined(CH_CFG_USE_DYNAMIC)
#define CH_CFG_USE_DYNAMIC                   TRUE
#endif

/** @} */

/*===========================================================================*/
/**
 * @name OSLIB options
 * @{
 */
/*===========================================================================*/

#if !defined(CH_CFG_USE_MAILBOXES)
#define CH_CFG_USE_MAILBOXES                 TRUE
#endif

/* >>> Ton choix conservé: pas de memchecks par défaut <<< */
#if !defined(CH_CFG_USE_MEMCHECKS)
#define CH_CFG_USE_MEMCHECKS                 FALSE
#endif

#if !defined(CH_CFG_USE_MEMCORE)
#define CH_CFG_USE_MEMCORE                   TRUE
#endif

#if !defined(CH_CFG_MEMCORE_SIZE)
#define CH_CFG_MEMCORE_SIZE                  0
#endif

#if !defined(CH_CFG_USE_HEAP)
#define CH_CFG_USE_HEAP                      TRUE
#endif

#if !defined(CH_CFG_USE_MEMPOOLS)
#define CH_CFG_USE_MEMPOOLS                  TRUE
#endif

#if !defined(CH_CFG_USE_OBJ_FIFOS)
#define CH_CFG_USE_OBJ_FIFOS                 TRUE
#endif

#if !defined(CH_CFG_USE_PIPES)
#define CH_CFG_USE_PIPES                     TRUE
#endif

#if !defined(CH_CFG_USE_OBJ_CACHES)
#define CH_CFG_USE_OBJ_CACHES                TRUE
#endif

#if !defined(CH_CFG_USE_DELEGATES)
#define CH_CFG_USE_DELEGATES                 TRUE
#endif

#if !defined(CH_CFG_USE_JOBS)
#define CH_CFG_USE_JOBS                      TRUE
#endif

/** @} */

/*===========================================================================*/
/**
 * @name Objects factory options
 * @{
 */
/*===========================================================================*/

#if !defined(CH_CFG_USE_FACTORY)
#define CH_CFG_USE_FACTORY                   TRUE
#endif

#if !defined(CH_CFG_FACTORY_MAX_NAMES_LENGTH)
#define CH_CFG_FACTORY_MAX_NAMES_LENGTH      8
#endif

#if !defined(CH_CFG_FACTORY_OBJECTS_REGISTRY)
#define CH_CFG_FACTORY_OBJECTS_REGISTRY      TRUE
#endif

#if !defined(CH_CFG_FACTORY_GENERIC_BUFFERS)
#define CH_CFG_FACTORY_GENERIC_BUFFERS       TRUE
#endif

#if !defined(CH_CFG_FACTORY_SEMAPHORES)
#define CH_CFG_FACTORY_SEMAPHORES            TRUE
#endif

#if !defined(CH_CFG_FACTORY_MAILBOXES)
#define CH_CFG_FACTORY_MAILBOXES             TRUE
#endif

#if !defined(CH_CFG_FACTORY_OBJ_FIFOS)
#define CH_CFG_FACTORY_OBJ_FIFOS             TRUE
#endif

#if !defined(CH_CFG_FACTORY_PIPES) || defined(__DOXYGEN__)
#define CH_CFG_FACTORY_PIPES                 TRUE
#endif

/** @} */

/*===========================================================================*/
/**
 * @name Debug options
 * @{
 */
/*===========================================================================*/

#if !defined(CH_DBG_STATISTICS)
#define CH_DBG_STATISTICS                    FALSE
#endif

#if !defined(CH_DBG_SYSTEM_STATE_CHECK)
#define CH_DBG_SYSTEM_STATE_CHECK            FALSE
#endif

#if !defined(CH_DBG_ENABLE_CHECKS)
#define CH_DBG_ENABLE_CHECKS                 FALSE
#endif

#if !defined(CH_DBG_ENABLE_ASSERTS)
#define CH_DBG_ENABLE_ASSERTS                FALSE
#endif

/* >>> Ton choix conservé: trace SLOW activée <<< */
#if !defined(CH_DBG_TRACE_MASK)
#define CH_DBG_TRACE_MASK                    CH_DBG_TRACE_MASK_SLOW
#endif

#if !defined(CH_DBG_TRACE_BUFFER_SIZE)
#define CH_DBG_TRACE_BUFFER_SIZE             128
#endif

/* >>> Ton choix conservé: stack check activé <<< */
#if !defined(CH_DBG_ENABLE_STACK_CHECK)
#define CH_DBG_ENABLE_STACK_CHECK            TRUE
#endif

#if !defined(CH_DBG_FILL_THREADS)
#if defined(BRICK_ENABLE_INSTRUMENTATION)
#define CH_DBG_FILL_THREADS                  TRUE
#else
#define CH_DBG_FILL_THREADS                  FALSE
#endif
#endif

#if !defined(CH_DBG_THREADS_PROFILING)
#define CH_DBG_THREADS_PROFILING             FALSE
#endif

/** @} */

/*===========================================================================*/
/**
 * @name Kernel hooks
 * @{
 */
/*===========================================================================*/

#define CH_CFG_SYSTEM_EXTRA_FIELDS \
  /* Add system custom fields here.*/

#define CH_CFG_SYSTEM_INIT_HOOK() { \
  /* Add system initialization code here.*/ \
}

#define CH_CFG_OS_INSTANCE_EXTRA_FIELDS \
  /* Add OS instance custom fields here.*/

#define CH_CFG_OS_INSTANCE_INIT_HOOK(oip) { \
  /* Add OS instance initialization code here.*/ \
}

#define CH_CFG_THREAD_EXTRA_FIELDS \
  /* Add threads custom fields here.*/

#define CH_CFG_THREAD_INIT_HOOK(tp) { \
  /* Add threads initialization code here.*/ \
}

#define CH_CFG_THREAD_EXIT_HOOK(tp) { \
  /* Add threads finalization code here.*/ \
}

#define CH_CFG_CONTEXT_SWITCH_HOOK(ntp, otp) { \
  /* Context switch code here.*/ \
}

#define CH_CFG_IRQ_PROLOGUE_HOOK() { \
  /* IRQ prologue code here.*/ \
}

#define CH_CFG_IRQ_EPILOGUE_HOOK() { \
  /* IRQ epilogue code here.*/ \
}

#define CH_CFG_IDLE_ENTER_HOOK() { \
  /* Idle-enter code here.*/ \
}

#define CH_CFG_IDLE_LEAVE_HOOK() { \
  /* Idle-leave code here.*/ \
}

#define CH_CFG_IDLE_LOOP_HOOK() { \
  /* Idle loop code here.*/ \
}

#define CH_CFG_SYSTEM_TICK_HOOK() { \
  /* System tick event code here.*/ \
}

#define CH_CFG_SYSTEM_HALT_HOOK(reason) { \
  /* System halt code here.*/ \
}

#define CH_CFG_TRACE_HOOK(tep) { \
  /* Trace code here.*/ \
}

#define CH_CFG_RUNTIME_FAULTS_HOOK(mask) { \
  /* Faults handling code here.*/ \
}

/** @} */

/*===========================================================================*/
/* Port-specific settings (override port settings defaulted in chcore.h).    */
/*===========================================================================*/

#endif  /* CHCONF_H */

/** @} */
