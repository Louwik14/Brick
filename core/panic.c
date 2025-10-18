#include "panic.h"

#include "ch.h"

#if defined(__GNUC__)
__attribute__((noreturn))
#endif
void panic(const char *message) {
    (void)message;
    chSysDisable();
    while (true) {
#if defined(__ARMCC_VERSION) || defined(__GNUC__)
        __NOP();
#else
        ;
#endif
    }
}
