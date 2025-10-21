#pragma once

#include "core/seq/seq_config.h"

#if SEQ_ENABLE_COLD_SECTIONS
#    define SEQ_COLD_SEC __attribute__((section(".cold")))
#else
#    define SEQ_COLD_SEC
#endif

#if SEQ_ENABLE_HOT_SECTIONS
#    define SEQ_HOT_SEC __attribute__((section(".hot")))
#else
#    define SEQ_HOT_SEC
#endif
