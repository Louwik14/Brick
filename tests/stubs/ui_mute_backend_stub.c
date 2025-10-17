#include <stdbool.h>
#include <stdint.h>
#include "ui/ui_mute_backend.h"

bool ui_mute_backend_is_muted(uint8_t track)
{
    (void)track;
    return false;
}
