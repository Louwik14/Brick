/**
 * @file core/ram_audit.h
 * @brief Helpers to tag global symbols for RAM footprint audits.
 */

#ifndef BRICK_CORE_RAM_AUDIT_H_
#define BRICK_CORE_RAM_AUDIT_H_

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__)
#define UI_RAM_AUDIT(sym)                                                        \
    __attribute__((used, section(".ui_ram_audit")))                             \
    static const char *const ui_ram_audit_entry_##sym = #sym
#else
#define UI_RAM_AUDIT(sym)
#endif

#ifdef __cplusplus
}
#endif

#endif /* BRICK_CORE_RAM_AUDIT_H_ */
