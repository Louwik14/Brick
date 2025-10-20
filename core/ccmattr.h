/**
 * @file ccmattr.h
 * @brief Attribute helper for CCRAM placement.
 *
 * Phase "CCRAM reset" defaults to an empty attribute so that all data
 * falls back to the system SRAM. Explicit opt-in can be re-enabled later
 * by redefining CCMRAM_ATTR before including this header.
 */

#ifndef CCMATTR_H
#define CCMATTR_H

#ifndef CCMRAM_ATTR
#define CCMRAM_ATTR
#endif

#endif /* CCMATTR_H */
