#ifndef BRICK_TEST_CHPRINTF_STUB_H
#define BRICK_TEST_CHPRINTF_STUB_H

#include <stdarg.h>
#include "ch.h"

static inline void chprintf(BaseSequentialStream *stream, const char *fmt, ...) {
    (void)stream;
    (void)fmt;
}

#endif /* BRICK_TEST_CHPRINTF_STUB_H */
