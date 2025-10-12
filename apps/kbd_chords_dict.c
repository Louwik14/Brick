/**
 * @file kbd_chords_dict.c
 * @brief Implémentation du dictionnaire d'accords + utilitaires de scale/transpose (inspiration Orchid).
 * @ingroup ui_apps
 */

#include "kbd_chords_dict.h"
#include <stddef.h>
#include <string.h>

/* Triades */
static const uint8_t TRIAD_MAJOR[] = {0, 4, 7};
static const uint8_t TRIAD_MINOR[] = {0, 3, 7};
static const uint8_t TRIAD_SUS4[]  = {0, 5, 7};
static const uint8_t TRIAD_DIM[]   = {0, 3, 6};
/* Extensions */
static const uint8_t EXT_7TH[]   = {10};
static const uint8_t EXT_MAJ7[]  = {11};
static const uint8_t EXT_6TH[]   = {9};
static const uint8_t EXT_9TH[]   = {14};

/* Utils set/tri */
static inline void add_unique(uint8_t *buf, uint8_t *n, uint8_t v, uint8_t maxn){
  if (*n >= maxn) return;
  for (uint8_t i=0;i<*n;++i) if (buf[i]==v) return;
  buf[(*n)++] = v;
}
static void add_all(const uint8_t *src, uint8_t m, uint8_t *dst, uint8_t *n, uint8_t maxn){
  for (uint8_t i=0;i<m;++i) add_unique(dst,n,src[i],maxn);
}
static void isort_u8(uint8_t *a, uint8_t n){
  for(uint8_t i=1;i<n;++i){ uint8_t k=a[i],j=i; while(j>0 && a[j-1]>k){a[j]=a[j-1];--j;} a[j]=k; }
}

/* API accords */
bool kbd_chords_dict_build(uint8_t chord_mask, uint8_t *intervals, uint8_t *count){
  if (!intervals || !count) return false;
  *count = 0;

  const uint8_t bases = (uint8_t)(chord_mask & KBD_CH_MASK_BASES);
  const uint8_t exts  = (uint8_t)(chord_mask & KBD_CH_MASK_EXTS);

  if (bases == 0) return false; /* extensions seules → invalide */

  if (bases & KBD_CH_BASE_MAJOR) add_all(TRIAD_MAJOR, sizeof(TRIAD_MAJOR), intervals, count, 12);
  if (bases & KBD_CH_BASE_MINOR) add_all(TRIAD_MINOR, sizeof(TRIAD_MINOR), intervals, count, 12);
  if (bases & KBD_CH_BASE_SUS4)  add_all(TRIAD_SUS4,  sizeof(TRIAD_SUS4),  intervals, count, 12);
  if (bases & KBD_CH_BASE_DIM)   add_all(TRIAD_DIM,   sizeof(TRIAD_DIM),   intervals, count, 12);

  if (exts & KBD_CH_EXT_7)    add_all(EXT_7TH,  sizeof(EXT_7TH),  intervals, count, 12);
  if (exts & KBD_CH_EXT_MAJ7) add_all(EXT_MAJ7, sizeof(EXT_MAJ7), intervals, count, 12);
  if (exts & KBD_CH_EXT_6)    add_all(EXT_6TH,  sizeof(EXT_6TH),  intervals, count, 12);
  if (exts & KBD_CH_EXT_9)    add_all(EXT_9TH,  sizeof(EXT_9TH),  intervals, count, 12);

  isort_u8(intervals, *count);
  return true;
}

/* API gammes (Note Zone 8 slots) */
int8_t kbd_scale_slot_semitone_offset(uint8_t scale_id, uint8_t slot){
  static const int8_t MAJOR[8]     = {0,2,4,5,7,9,11,12};
  static const int8_t NAT_MIN[8]   = {0,2,3,5,7,8,10,12};
  static const int8_t DORIAN[8]    = {0,2,3,5,7,9,10,12};
  static const int8_t MIXO[8]      = {0,2,4,5,7,9,10,12};
  static const int8_t PENT_MAJ[8]  = {0,2,4,7,9,12,14,16};
  static const int8_t PENT_MIN[8]  = {0,3,5,7,10,12,15,17};
  static const int8_t CHROMA[8]    = {0,1,2,3,4,5,6,12};

  const uint8_t s = (uint8_t)(slot & 7u);
  switch (scale_id){
    case KBD_SCALE_ID_MAJOR:      return MAJOR[s];
    case KBD_SCALE_ID_NAT_MINOR:  return NAT_MIN[s];
    case KBD_SCALE_ID_DORIAN:     return DORIAN[s];
    case KBD_SCALE_ID_MIXOLYDIAN: return MIXO[s];
    case KBD_SCALE_ID_PENT_MAJOR: return PENT_MAJ[s];
    case KBD_SCALE_ID_PENT_MINOR: return PENT_MIN[s];
    case KBD_SCALE_ID_CHROMATIC:  return CHROMA[s];
    default:                      return MAJOR[s];
  }
}

#ifdef TEST
static bool arr_eq(const uint8_t *a,const uint8_t *b,uint8_t n){ return memcmp(a,b,n)==0; }
bool kbd_chords_dict_selftest(void){
  { uint8_t iv[12],n; if (kbd_chords_dict_build(KBD_CH_EXT_7,iv,&n)) return false; }
  { uint8_t iv[12],n; if (!kbd_chords_dict_build(KBD_CH_BASE_MAJOR,iv,&n)) return false;
    const uint8_t exp[]={0,4,7}; if (n!=3||!arr_eq(iv,exp,3)) return false; }
  { uint8_t iv[12],n; if (!kbd_chords_dict_build(KBD_CH_BASE_MINOR|KBD_CH_EXT_7,iv,&n)) return false;
    const uint8_t exp[]={0,3,7,10}; if (n!=4||!arr_eq(iv,exp,4)) return false; }
  { uint8_t iv[12],n; if (!kbd_chords_dict_build(KBD_CH_BASE_MAJOR|KBD_CH_EXT_MAJ7|KBD_CH_EXT_9,iv,&n)) return false;
    const uint8_t exp[]={0,4,7,11,14}; if (n!=5||!arr_eq(iv,exp,5)) return false; }
  return true;
}
#endif
