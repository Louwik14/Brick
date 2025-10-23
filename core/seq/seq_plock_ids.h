#pragma once

#include <stdint.h>

// Plage 0x00–0x3F : P-locks "internes" (SEQ/MIDI internes)
// Plage 0x40–0xFF : P-locks "cartouche" (cart_proto)
static inline int      pl_is_midi(uint8_t id)  { return id < 0x40; }
static inline int      pl_is_cart(uint8_t id)  { return id >= 0x40; }
static inline uint8_t  pl_cart_id(uint8_t id)  { return (uint8_t)(id - 0x40); }

// Encodage signé compact pour offsets All / micro / etc.
static inline uint8_t  pl_u8_from_s8(int8_t v) { return (uint8_t)((int16_t)v + 128); }
static inline int8_t   pl_s8_from_u8(uint8_t u){ return (int8_t)((int16_t)u - 128); }

// Esquisse d'IDs internes (sera affinée quand on branchera le pool)
enum {
  // Offsets "All" (signés via helpers ci-dessus)
  PL_INT_ALL_TRANSP = 0x00,   // s8
  PL_INT_ALL_VEL    = 0x01,   // s8 (offset)
  PL_INT_ALL_LEN    = 0x02,   // s8 (offset)
  PL_INT_ALL_MIC    = 0x03,   // s8

  // Paramètres par voix (temp : base + index voix 0..3)
  PL_INT_NOTE_V0 = 0x08, PL_INT_NOTE_V1 = 0x09, PL_INT_NOTE_V2 = 0x0A, PL_INT_NOTE_V3 = 0x0B,
  PL_INT_VEL_V0  = 0x0C, PL_INT_VEL_V1  = 0x0D, PL_INT_VEL_V2  = 0x0E, PL_INT_VEL_V3  = 0x0F,
  PL_INT_LEN_V0  = 0x10, PL_INT_LEN_V1  = 0x11, PL_INT_LEN_V2  = 0x12, PL_INT_LEN_V3  = 0x13,
  PL_INT_MIC_V0  = 0x14, PL_INT_MIC_V1  = 0x15, PL_INT_MIC_V2  = 0x16, PL_INT_MIC_V3  = 0x17,

  PL_INT__RESERVED_END = 0x3F
};

