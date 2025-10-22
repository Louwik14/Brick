#pragma once
#include <stdint.h>

/* RTOS shim côté apps :
   Fournit uniquement systime_t pour compiler sans ChibiOS.
   Aucun appel de synchronisation, mutex ou thread.
   Compile sur host et cible sans dépendre du RTOS. */

typedef uint32_t systime_t;
