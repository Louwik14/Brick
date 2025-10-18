#include "panic.h"

void HardFault_Handler(void) {
    panic("HardFault");
}
