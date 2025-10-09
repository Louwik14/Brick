/**
 * @file cart_xva1_spec.c
 * @brief Spécification complète de la cartouche **XVA1** (synthétiseur virtuel).
 *
 * Contient :
 *  - Tables de labels XVA1 (formes d’onde, filtres, LFO, FX, etc.)
 *  - Définition des menus (`ui_menu_spec_t`)
 *  - Cartouche `CART_XVA1` (structure `ui_cart_spec_t`)
 *
 * @ingroup cart
 *
 * @note Les labels strictement universels (par ex. "On/Off") proviennent
 *       de `ui_labels_common.h` et sont référencés via des alias sémantiques.
 */

#include "cart_xva1_spec.h"
#include "ui_labels_common.h"   /* ui_labels_onoff, ui_labels_basic_waves */

/* ==========================================================================
 *   Tables de labels globales spécifiques XVA1
 *   (Conservent les contenus "exotiques" propres au moteur XVA1)
 * ========================================================================== */

/* On/Off local (gardé pour compat, mais aliasé vers commun via LBL_BOOL) */
const char* const onOff[2]        = { "Off", "On" };

const char* const filterTypes[22] = {
  "Bypass","1pLP","2pLP","3pLP","4pLP",
  "1pHP","2pHP","3pHP","4pHP","2pBP",
  "4pBP","2pBR","4pBR","2pLP2LP","2pLP2BP",
  "2pLP2HP","2xLP","LP+BP","LP+HP","BP+BP",
  "BP+HP","HP+HP"
};

const char* const oscWaves[9] = {
  "SawUp","SawDn","Square","Tri","Sine","Noise","Stk3","Stk7m","Stk7s"
};

const char* const lfoWaves[10] = {
  "Tri","Sqr","SawU","SawD","Sine","S+S2","S+S3","S³","Gtr","Rnd"
};

const char* const distType[4]    = { "HrdCp","SftCp","12AX","DSL" };
const char* const fxRouting[3]   = { "Std","Alt","Off" };
const char* const reverbModes[2] = { "Plate","Hall" };
const char* const delayModes[3]  = { "Stereo","Cross","Bounce" };
const char* const chorusModes[4] = { "ChL","ChS","FlL","FlS" };
const char* const phaserModes[3] = { "Mono","Stereo","Cross" };

/* ==========================================================================
 *  Labels internes (utilisés uniquement ici)
 * ========================================================================== */
static const char* const sync[2]       = { "Free","Sync" };
static const char* const keytrack[2]   = { "Keytrk","Fixed" };
static const char* const routing[3]    = { "Paral","Indep","Bypass" };

static const char* const lfoSync[4]    = { "1FR","1KS","MFR","MKS" };
static const char* const egLoop[2]     = { "LoopOff","LoopOn" };
static const char* const egLoopSeg[2]  = { "ToAtk","ToDcy" };
static const char* const egRst[2]      = { "Keep","Rst" };
static const char* const legato[2]     = { "Poly","Mono" };
static const char* const portaMode[3]  = { "Off","Alw","Fngr" };

static const char* const gateCurve[2]  = { "S1","S2" };
static const char* const gain[4]       = { "0dB","+6dB","+12dB","+18dB" };
static const char* const phase[4]      = { "0°","90°","180°","270°" };
static const char* const drive[8]      = { "0","1","2","3","4","5","6","7" };
static const char* const bandw[8]      = {
  "Full 48kHz","20 kHz","18 kHz","16 kHz","14 kHz","12 kHz","10 kHz","8 kHz"
};

/* ==========================================================================
 *  Aliases sémantiques
 *  - LBL_BOOL pointe sur les labels communs (On/Off universel)
 *  - Les autres restent mappés vers les tables XVA1 spécifiques
 * ========================================================================== */
#define LBL_BOOL            ui_labels_onoff

/* ==========================================================================
 *  Compatibilité (anciens alias, réorientés au besoin)
 * ========================================================================== */
#define waveLabels        oscWaves
#define onOffLabels       LBL_BOOL       /* <- avant: onOff */
#define syncLabels        sync
#define keytrackLabels    keytrack
#define filterTypeLabels  filterTypes
#define routingLabels     routing
#define lfoWaveLabels     lfoWaves
#define lfoSyncLabels     lfoSync
#define egLoopLabels      egLoop
#define egLoopSegLabels   egLoopSeg
#define egRstLabels       egRst
#define legatoLabels      legato
#define portaModeLabels   portaMode
#define distTypeLabels    distType
#define gateCurveLabels   gateCurve
#define gainLabels        gain
#define fxRoutingLabels   fxRouting
#define reverbModeLabels  reverbModes
#define delayModeLabels   delayModes
#define chorusModeLabels  chorusModes
#define phaserModeLabels  phaserModes
#define phaseLabels       phase
#define driveLabels       drive
#define bandwLabels       bandw

/* ==========================================================================
 *  Raccourci : page vide
 * ========================================================================== */
#define EMPTY_PAGE  { .params = { {0},{0},{0},{0} } }

/* ==========================================================================
 * MENUS XVA1
 * ========================================================================== */

/* ------------------------------ OSC1 ------------------------------------ */
const ui_menu_spec_t XVA1_MENU_OSC1 = {
  .name = "OSC1",
  .page_titles = { "Struct","Timbre","Struct","Lvls","Mod" },
  .pages = {
    { .params = {
        { "On/Off", UI_PARAM_BOOL,  1,   0, .meta.en={LBL_BOOL,2}, .is_bitwise=1, .bit_mask=0x01 },
        { "Wave",   UI_PARAM_ENUM, 11,   0, .meta.en={lfoWaves,9} },
        { "Transp", UI_PARAM_CONT, 19,   0, .meta.range={0,255,1} },
        { "Tune",   UI_PARAM_CONT, 23,   0, .meta.range={0,255,1} },
    } },
    { .params = {
        { "Pwidth", UI_PARAM_CONT, 15,   0, .meta.range={0,255,1} },
        { "SawTune",UI_PARAM_CONT, 285,  0, .meta.range={0,255,1} },
        { "Drift",  UI_PARAM_CONT, 260,  0, .meta.range={0,255,1} },
        { "Sync",   UI_PARAM_BOOL, 5,    0, .meta.en={LBL_BOOL,2}, .is_bitwise=1, .bit_mask=0x01 },
    } },
    { .params = {
        { "Phase",  UI_PARAM_CONT, 7,    0, .meta.range={0,255,1} },
        { "Ktrk",   UI_PARAM_BOOL, 6,    0, .meta.en={LBL_BOOL,2}, .is_bitwise=1, .bit_mask=0x01 },
        { "-",      UI_PARAM_CONT, 0,    0, .meta.range={0,0,0} },
        { "-",      UI_PARAM_CONT, 0,    0, .meta.range={0,0,0} },
    } },
    { .params = {
        { "Lvl",     UI_PARAM_CONT, 27,  0, .meta.range={0,255,1} },
        { "Lvl L",   UI_PARAM_CONT, 31,  0, .meta.range={0,255,1} },
        { "Lvl R",   UI_PARAM_CONT, 32,  0, .meta.range={0,255,1} },
        { "VeloSens",UI_PARAM_CONT, 39,  0, .meta.range={0,255,1} },
    } },
    { .params = {
        { "Ams", UI_PARAM_CONT, 67,  0, .meta.range={0,255,1} },
        { "Pms", UI_PARAM_CONT, 63,  0, .meta.range={0,255,1} },
        { "-",   UI_PARAM_CONT, 0,   0, .meta.range={0,0,0} },
        { "-",   UI_PARAM_CONT, 0,   0, .meta.range={0,0,0} },
    } },
  }
};

/* ------------------------------ OSC2 ------------------------------------ */
const ui_menu_spec_t XVA1_MENU_OSC2 = {
  .name = "OSC2",
  .page_titles = { "Struct","Timbre","Struct","Lvls","Mod" },
  .pages = {
    { .params = {
        { "On/Off", UI_PARAM_BOOL,  2,   0, .meta.en={LBL_BOOL,2}, .is_bitwise=1, .bit_mask=0x01 },
        { "Wave",   UI_PARAM_ENUM, 12,   0, .meta.en={lfoWaves,9} },
        { "Transp", UI_PARAM_CONT, 20,   0, .meta.range={0,255,1} },
        { "Tune",   UI_PARAM_CONT, 24,   0, .meta.range={0,255,1} },
    } },
    { .params = {
        { "Pwidth", UI_PARAM_CONT, 16,   0, .meta.range={0,255,1} },
        { "SawTune",UI_PARAM_CONT, 286,  0, .meta.range={0,255,1} },
        { "Drift",  UI_PARAM_CONT, 261,  0, .meta.range={0,255,1} },
        { "Sync",   UI_PARAM_BOOL, 5,    0, .meta.en={LBL_BOOL,2}, .is_bitwise=1, .bit_mask=0x02 },
    } },
    { .params = {
        { "Phase",  UI_PARAM_CONT, 8,    0, .meta.range={0,255,1} },
        { "Ktrk",   UI_PARAM_BOOL, 6,    0, .meta.en={LBL_BOOL,2}, .is_bitwise=1, .bit_mask=0x02 },
        { "-",      UI_PARAM_CONT, 0,    0, .meta.range={0,0,0} },
        { "-",      UI_PARAM_CONT, 0,    0, .meta.range={0,0,0} },
    } },
    { .params = {
        { "Lvl",     UI_PARAM_CONT, 28,  0, .meta.range={0,255,1} },
        { "Lvl L",   UI_PARAM_CONT, 33,  0, .meta.range={0,255,1} },
        { "Lvl R",   UI_PARAM_CONT, 34,  0, .meta.range={0,255,1} },
        { "VeloSens",UI_PARAM_CONT, 40,  0, .meta.range={0,255,1} },
    } },
    { .params = {
        { "Ams", UI_PARAM_CONT, 68,  0, .meta.range={0,255,1} },
        { "Pms", UI_PARAM_CONT, 64,  0, .meta.range={0,255,1} },
        { "-",   UI_PARAM_CONT, 0,   0, .meta.range={0,0,0} },
        { "-",   UI_PARAM_CONT, 0,   0, .meta.range={0,0,0} },
    } },
  }
};

/* ------------------------------ OSC3 ------------------------------------ */
const ui_menu_spec_t XVA1_MENU_OSC3 = {
  .name = "OSC3",
  .page_titles = { "Struct","Timbre","Struct","Lvls","Mod" },
  .pages = {
    { .params = {
        { "On/Off", UI_PARAM_BOOL,  3,   0, .meta.en={LBL_BOOL,2}, .is_bitwise=1, .bit_mask=0x01 },
        { "Wave",   UI_PARAM_ENUM, 13,   0, .meta.en={lfoWaves,9} },
        { "Transp", UI_PARAM_CONT, 21,   0, .meta.range={0,255,1} },
        { "Tune",   UI_PARAM_CONT, 25,   0, .meta.range={0,255,1} },
    } },
    { .params = {
        { "Pwidth", UI_PARAM_CONT, 17,   0, .meta.range={0,255,1} },
        { "SawTune",UI_PARAM_CONT, 287,  0, .meta.range={0,255,1} },
        { "Drift",  UI_PARAM_CONT, 262,  0, .meta.range={0,255,1} },
        { "Sync",   UI_PARAM_BOOL, 5,    0, .meta.en={LBL_BOOL,2}, .is_bitwise=1, .bit_mask=0x04 },
    } },
    { .params = {
        { "Phase",  UI_PARAM_CONT, 9,    0, .meta.range={0,255,1} },
        { "Ktrk",   UI_PARAM_BOOL, 6,    0, .meta.en={LBL_BOOL,2}, .is_bitwise=1, .bit_mask=0x04 },
        { "Ring",   UI_PARAM_BOOL, 271,  0, .meta.en={LBL_BOOL,2}, .is_bitwise=1, .bit_mask=0x01 },
        { "-",      UI_PARAM_CONT, 0,    0, .meta.range={0,0,0} },
    } },
    { .params = {
        { "Lvl",     UI_PARAM_CONT, 29,  0, .meta.range={0,255,1} },
        { "Lvl L",   UI_PARAM_CONT, 35,  0, .meta.range={0,255,1} },
        { "Lvl R",   UI_PARAM_CONT, 36,  0, .meta.range={0,255,1} },
        { "VeloSens",UI_PARAM_CONT, 41,  0, .meta.range={0,255,1} },
    } },
    { .params = {
        { "Ams", UI_PARAM_CONT, 69,  0, .meta.range={0,255,1} },
        { "Pms", UI_PARAM_CONT, 65,  0, .meta.range={0,255,1} },
        { "-",   UI_PARAM_CONT, 0,   0, .meta.range={0,0,0} },
        { "-",   UI_PARAM_CONT, 0,   0, .meta.range={0,0,0} },
    } },
  }
};

/* ------------------------------ OSC4 ------------------------------------ */
const ui_menu_spec_t XVA1_MENU_OSC4 = {
  .name = "OSC4",
  .page_titles = { "Struct","Timbre","Struct","Lvls","Mod" },
  .pages = {
    { .params = {
        { "On/Off", UI_PARAM_BOOL,  4,   0, .meta.en={LBL_BOOL,2}, .is_bitwise=1, .bit_mask=0x01 },
        { "Wave",   UI_PARAM_ENUM, 14,   0, .meta.en={lfoWaves,9} },
        { "Transp", UI_PARAM_CONT, 22,   0, .meta.range={0,255,1} },
        { "Tune",   UI_PARAM_CONT, 26,   0, .meta.range={0,255,1} },
    } },
    { .params = {
        { "Pwidth", UI_PARAM_CONT, 18,   0, .meta.range={0,255,1} },
        { "SawTune",UI_PARAM_CONT, 288,  0, .meta.range={0,255,1} },
        { "Drift",  UI_PARAM_CONT, 263,  0, .meta.range={0,255,1} },
        { "Sync",   UI_PARAM_BOOL, 5,    0, .meta.en={LBL_BOOL,2}, .is_bitwise=1, .bit_mask=0x08 },
    } },
    { .params = {
        { "Phase",  UI_PARAM_CONT, 10,   0, .meta.range={0,255,1} },
        { "Ktrk",   UI_PARAM_BOOL, 6,    0, .meta.en={LBL_BOOL,2}, .is_bitwise=1, .bit_mask=0x08 },
        { "Ring",   UI_PARAM_BOOL, 272,  0, .meta.en={LBL_BOOL,2}, .is_bitwise=1, .bit_mask=0x02 },
        { "-",      UI_PARAM_CONT, 0,    0, .meta.range={0,0,0} },
    } },
    { .params = {
        { "Lvl",     UI_PARAM_CONT, 30,  0, .meta.range={0,255,1} },
        { "Lvl L",   UI_PARAM_CONT, 37,  0, .meta.range={0,255,1} },
        { "Lvl R",   UI_PARAM_CONT, 38,  0, .meta.range={0,255,1} },
        { "VeloSens",UI_PARAM_CONT, 42,  0, .meta.range={0,255,1} },
    } },
    { .params = {
        { "Ams", UI_PARAM_CONT, 70,  0, .meta.range={0,255,1} },
        { "Pms", UI_PARAM_CONT, 66,  0, .meta.range={0,255,1} },
        { "-",   UI_PARAM_CONT, 0,   0, .meta.range={0,0,0} },
        { "-",   UI_PARAM_CONT, 0,   0, .meta.range={0,0,0} },
    } },
  }
};

/* ------------------------------ FILTER ---------------------------------- */
const ui_menu_spec_t XVA1_MENU_FILTER = {
  .name = "Filter",
  .page_titles = { "Main","Sub","Mod","RMod","-" },
  .pages = {
    { .params = {
        { "Type", UI_PARAM_ENUM, 71, 0, .meta.en={filterTypeLabels,22} },
        { "Cut1", UI_PARAM_CONT, 72, 0, .meta.range={0,255,1} },
        { "Res1", UI_PARAM_CONT, 77, 0, .meta.range={0,255,1} },
        { "Eg",   UI_PARAM_CONT, 75, 0, .meta.range={0,255,1} },
    } },
    { .params = {
        { "Cut2",  UI_PARAM_CONT, 78,  0, .meta.range={0,255,1} },
        { "Res2",  UI_PARAM_CONT, 79,  0, .meta.range={0,255,1} },
        { "Drive", UI_PARAM_ENUM, 275, 0, .meta.en={driveLabels,8} },
        { "Route", UI_PARAM_ENUM, 278, 0, .meta.en={routingLabels,3} },
    } },
    { .params = {
        { "Vel",   UI_PARAM_CONT, 73,  0, .meta.range={0,255,1} },
        { "KtCut", UI_PARAM_CONT, 74,  0, .meta.range={0,255,1} },
        { "EgVel", UI_PARAM_CONT, 76,  0, .meta.range={0,255,1} },
        { "-",     UI_PARAM_CONT, 0,   0, .meta.range={0,0,0} },
    } },
    { .params = {
        { "VlR",   UI_PARAM_CONT, 276, 0, .meta.range={0,255,1} },
        { "KtRes", UI_PARAM_CONT, 277, 0, .meta.range={0,255,1} },
        { "-",     UI_PARAM_CONT, 0,   0, .meta.range={0,0,0} },
        { "-",     UI_PARAM_CONT, 0,   0, .meta.range={0,0,0} },
    } },
    EMPTY_PAGE
  }
};

/* ------------------------------ LFO 1 & 2 ------------------------------- */
const ui_menu_spec_t XVA1_MENU_LFO12 = {
  .name = "LFO 1 & 2",
  .page_titles = { "Main1","Mod1","Main2","Mod2","-" },
  .pages = {
    { .params = {
        { "Wave",  UI_PARAM_ENUM, 160, 0, .meta.en={lfoWaveLabels,10} },
        { "Range", UI_PARAM_CONT, 166, 0, .meta.range={0,255,1} },
        { "Speed", UI_PARAM_CONT, 161, 0, .meta.range={0,255,1} },
        { "Sync",  UI_PARAM_ENUM, 162, 0, .meta.en={lfoSyncLabels,4} },
    } },
    { .params = {
        { "Fade",  UI_PARAM_CONT, 163, 0, .meta.range={0,255,1} },
        { "Pitch", UI_PARAM_CONT, 164, 0, .meta.range={0,255,1} },
        { "Amp",   UI_PARAM_CONT, 260, 0, .meta.range={0,255,1} },
        { "-",     UI_PARAM_CONT, 0,   0, .meta.range={0,0,0} },
    } },
    { .params = {
        { "Wave",  UI_PARAM_ENUM, 170, 0, .meta.en={lfoWaveLabels,10} },
        { "Range", UI_PARAM_CONT, 176, 0, .meta.range={0,255,1} },
        { "Speed", UI_PARAM_CONT, 171, 0, .meta.range={0,255,1} },
        { "Sync",  UI_PARAM_ENUM, 172, 0, .meta.en={lfoSyncLabels,4} },
    } },
    { .params = {
        { "Fade",   UI_PARAM_CONT, 173, 0, .meta.range={0,255,1} },
        { "Pw",     UI_PARAM_CONT, 174, 0, .meta.range={0,255,1} },
        { "Cutoff", UI_PARAM_CONT, 175, 0, .meta.range={0,255,1} },
        { "-",      UI_PARAM_CONT, 0,   0, .meta.range={0,0,0} },
    } },
    EMPTY_PAGE
  }
};
/* ------------------------------ LFO MIDI MOD ---------------------------- */
const ui_menu_spec_t XVA1_MENU_LFO_MIDIMOD = {
  .name = "LFO Midi Mod",
  .page_titles = { "AftEr","Wheel","CC02","CC04","-" },
  .pages = {
    { .params = {
        { "Amp",    UI_PARAM_CONT, 192, 0, .meta.range={0,255,1} },
        { "Cutoff", UI_PARAM_CONT, 188, 0, .meta.range={0,255,1} },
        { "Pulse",  UI_PARAM_CONT, 184, 0, .meta.range={0,255,1} },
        { "Pitch",  UI_PARAM_CONT, 180, 0, .meta.range={0,255,1} },
    } },
    { .params = {
        { "Amp",    UI_PARAM_CONT, 193, 0, .meta.range={0,255,1} },
        { "Cutoff", UI_PARAM_CONT, 189, 0, .meta.range={0,255,1} },
        { "Pulse",  UI_PARAM_CONT, 185, 0, .meta.range={0,255,1} },
        { "Pitch",  UI_PARAM_CONT, 181, 0, .meta.range={0,255,1} },
    } },
    { .params = {
        { "Amp",    UI_PARAM_CONT, 194, 0, .meta.range={0,255,1} },
        { "Cutoff", UI_PARAM_CONT, 190, 0, .meta.range={0,255,1} },
        { "Pulse",  UI_PARAM_CONT, 186, 0, .meta.range={0,255,1} },
        { "Pitch",  UI_PARAM_CONT, 182, 0, .meta.range={0,255,1} },
    } },
    { .params = {
        { "Amp",    UI_PARAM_CONT, 195, 0, .meta.range={0,255,1} },
        { "Cutoff", UI_PARAM_CONT, 191, 0, .meta.range={0,255,1} },
        { "Pulse",  UI_PARAM_CONT, 187, 0, .meta.range={0,255,1} },
        { "Pitch",  UI_PARAM_CONT, 183, 0, .meta.range={0,255,1} },
    } },
    EMPTY_PAGE
  }
};

/* ------------------------------ MIDI GLOBAL MOD ------------------------- */
const ui_menu_spec_t XVA1_MENU_MIDI_GLOBAL = {
  .name = "Midi Global Mod",
  .page_titles = { "After","Wheel","CC02","CC04","Randm" },
  .pages = {
    { .params = {
        { "Amp",    UI_PARAM_CONT, 212, 0, .meta.range={0,255,1} },
        { "Cutoff", UI_PARAM_CONT, 208, 0, .meta.range={0,255,1} },
        { "Pulse",  UI_PARAM_CONT, 204, 0, .meta.range={0,255,1} },
        { "Pitch",  UI_PARAM_CONT, 200, 0, .meta.range={0,255,1} },
    } },
    { .params = {
        { "Amp",    UI_PARAM_CONT, 213, 0, .meta.range={0,255,1} },
        { "Cutoff", UI_PARAM_CONT, 209, 0, .meta.range={0,255,1} },
        { "Pulse",  UI_PARAM_CONT, 205, 0, .meta.range={0,255,1} },
        { "Pitch",  UI_PARAM_CONT, 201, 0, .meta.range={0,255,1} },
    } },
    { .params = {
        { "Amp",    UI_PARAM_CONT, 214, 0, .meta.range={0,255,1} },
        { "Cutoff", UI_PARAM_CONT, 210, 0, .meta.range={0,255,1} },
        { "Pulse",  UI_PARAM_CONT, 206, 0, .meta.range={0,255,1} },
        { "Pitch",  UI_PARAM_CONT, 202, 0, .meta.range={0,255,1} },
    } },
    { .params = {
        { "Amp",    UI_PARAM_CONT, 215, 0, .meta.range={0,255,1} },
        { "Cutoff", UI_PARAM_CONT, 211, 0, .meta.range={0,255,1} },
        { "Pulse",  UI_PARAM_CONT, 207, 0, .meta.range={0,255,1} },
        { "Pitch",  UI_PARAM_CONT, 203, 0, .meta.range={0,255,1} },
    } },
    { .params = {
        { "Amp",    UI_PARAM_CONT, 216, 0, .meta.range={0,255,1} },
        { "Cutoff", UI_PARAM_CONT, 217, 0, .meta.range={0,255,1} },
        { "Pulse",  UI_PARAM_CONT, 218, 0, .meta.range={0,255,1} },
        { "Pitch",  UI_PARAM_CONT, 219, 0, .meta.range={0,255,1} },
    } },
  }
};

/* ------------------------------ Filter ENV ------------------------------ */
const ui_menu_spec_t XVA1_MENU_ENV_FILTER = {
  .name = "Filter ENV",
  .page_titles = { "ADSR","ENV+","Env++","ModEG","-" },
  .pages = {
    { .params = {
        { "Attack",  UI_PARAM_CONT, 116, 0, .meta.range={0,255,1} },
        { "Decay",   UI_PARAM_CONT, 121, 0, .meta.range={0,255,1} },
        { "Sust",    UI_PARAM_CONT, 96,  0, .meta.range={0,255,1} },
        { "Release", UI_PARAM_CONT, 131, 0, .meta.range={0,255,1} },
    } },
    { .params = {
        { "Start",  UI_PARAM_CONT, 81,  0, .meta.range={0,255,1} },
        { "AtkShp", UI_PARAM_ENUM, 82,  0, .meta.en={gainLabels,4} },
        { "AtkMax", UI_PARAM_CONT, 83,  0, .meta.range={0,255,1} },
        { "Decay2", UI_PARAM_CONT, 84,  0, .meta.range={0,255,1} },
    } },
    { .params = {
        { "Dcy2Lv", UI_PARAM_CONT, 85,  0, .meta.range={0,255,1} },
        { "RelShp", UI_PARAM_ENUM, 86,  0, .meta.en={gainLabels,4} },
        { "Init2",  UI_PARAM_CONT, 87,  0, .meta.range={0,255,1} },
        { "Atk2",   UI_PARAM_CONT, 88,  0, .meta.range={0,255,1} },
    } },
    { .params = {
        { "VelAtk", UI_PARAM_CONT, 97,  0, .meta.range={0,255,1} },
        { "VelDec", UI_PARAM_CONT, 98,  0, .meta.range={0,255,1} },
        { "VelRel", UI_PARAM_CONT, 99,  0, .meta.range={0,255,1} },
        { "KeyTrk", UI_PARAM_CONT, 100, 0, .meta.range={0,255,1} },
    } },
    EMPTY_PAGE
  }
};

/* ------------------------------ AMP ENV --------------------------------- */
const ui_menu_spec_t XVA1_MENU_ENV_AMP = {
  .name = "Amp ENV",
  .page_titles = { "ADSR","ENV+","Env++","ModEG","-" },
  .pages = {
    { .params = {
        { "Attack",  UI_PARAM_CONT, 101, 0, .meta.range={0,255,1} },
        { "Decay",   UI_PARAM_CONT, 102, 0, .meta.range={0,255,1} },
        { "Sust",    UI_PARAM_CONT, 103, 0, .meta.range={0,255,1} },
        { "Release", UI_PARAM_CONT, 104, 0, .meta.range={0,255,1} },
    } },
    { .params = {
        { "Start",  UI_PARAM_CONT, 105, 0, .meta.range={0,255,1} },
        { "AtkShp", UI_PARAM_ENUM, 106, 0, .meta.en={gainLabels,4} },
        { "AtkMax", UI_PARAM_CONT, 107, 0, .meta.range={0,255,1} },
        { "Decay2", UI_PARAM_CONT, 108, 0, .meta.range={0,255,1} },
    } },
    { .params = {
        { "Dcy2Lv", UI_PARAM_CONT, 109, 0, .meta.range={0,255,1} },
        { "RelShp", UI_PARAM_ENUM, 110, 0, .meta.en={gainLabels,4} },
        { "Init2",  UI_PARAM_CONT, 111, 0, .meta.range={0,255,1} },
        { "Atk2",   UI_PARAM_CONT, 112, 0, .meta.range={0,255,1} },
    } },
    { .params = {
        { "VelAtk", UI_PARAM_CONT, 113, 0, .meta.range={0,255,1} },
        { "VelDec", UI_PARAM_CONT, 114, 0, .meta.range={0,255,1} },
        { "VelRel", UI_PARAM_CONT, 115, 0, .meta.range={0,255,1} },
        { "KeyTrk", UI_PARAM_CONT, 118, 0, .meta.range={0,255,1} },
    } },
    EMPTY_PAGE
  }
};

/* ------------------------------ PITCH ENV ------------------------------- */
const ui_menu_spec_t XVA1_MENU_ENV_PITCH = {
  .name = "Pitch ENV",
  .page_titles = { "ADSR","ENV+","Env++","ModEG","-" },
  .pages = {
    { .params = {
        { "Attack",  UI_PARAM_CONT, 120, 0, .meta.range={0,255,1} },
        { "Decay",   UI_PARAM_CONT, 122, 0, .meta.range={0,255,1} },
        { "Sust",    UI_PARAM_CONT, 123, 0, .meta.range={0,255,1} },
        { "Release", UI_PARAM_CONT, 124, 0, .meta.range={0,255,1} },
    } },
    { .params = {
        { "Start",  UI_PARAM_CONT, 125, 0, .meta.range={0,255,1} },
        { "AtkShp", UI_PARAM_ENUM, 126, 0, .meta.en={gainLabels,4} },
        { "AtkMax", UI_PARAM_CONT, 127, 0, .meta.range={0,255,1} },
        { "Decay2", UI_PARAM_CONT, 128, 0, .meta.range={0,255,1} },
    } },
    { .params = {
        { "Dcy2Lv", UI_PARAM_CONT, 129, 0, .meta.range={0,255,1} },
        { "RelShp", UI_PARAM_ENUM, 130, 0, .meta.en={gainLabels,4} },
        { "Init2",  UI_PARAM_CONT, 132, 0, .meta.range={0,255,1} },
        { "Atk2",   UI_PARAM_CONT, 133, 0, .meta.range={0,255,1} },
    } },
    { .params = {
        { "VelAtk", UI_PARAM_CONT, 134, 0, .meta.range={0,255,1} },
        { "VelDec", UI_PARAM_CONT, 135, 0, .meta.range={0,255,1} },
        { "VelRel", UI_PARAM_CONT, 136, 0, .meta.range={0,255,1} },
        { "KeyTrk", UI_PARAM_CONT, 137, 0, .meta.range={0,255,1} },
    } },
    EMPTY_PAGE
  }
};

/* ------------------------------ FX1 (DIST) ------------------------------ */
const ui_menu_spec_t XVA1_MENU_FX1 = {
  .name = "FX1 Dist",
  .page_titles = { "Main","Tone","Gate","Pan","-" },
  .pages = {
    { .params = {
        { "Type",  UI_PARAM_ENUM, 320, 0, .meta.en={distTypeLabels,4} },
        { "Drive", UI_PARAM_CONT, 321, 0, .meta.range={0,255,1} },
        { "Gate",  UI_PARAM_ENUM, 322, 0, .meta.en={gateCurveLabels,2} },
        { "Mix",   UI_PARAM_CONT, 323, 0, .meta.range={0,255,1} },
    } },
    { .params = {
        { "Tone", UI_PARAM_CONT, 324, 0, .meta.range={0,255,1} },
        { "HP",   UI_PARAM_CONT, 325, 0, .meta.range={0,255,1} },
        { "LP",   UI_PARAM_CONT, 326, 0, .meta.range={0,255,1} },
        { "-",    UI_PARAM_CONT, 0,   0, .meta.range={0,0,0} },
    } },
    { .params = {
        { "Gate", UI_PARAM_CONT, 327, 0, .meta.range={0,255,1} },
        { "Atk",  UI_PARAM_CONT, 328, 0, .meta.range={0,255,1} },
        { "Rel",  UI_PARAM_CONT, 329, 0, .meta.range={0,255,1} },
        { "-",    UI_PARAM_CONT, 0,   0, .meta.range={0,0,0} },
    } },
    { .params = {
        { "Pan",    UI_PARAM_CONT, 330, 0, .meta.range={0,255,1} },
        { "Width",  UI_PARAM_CONT, 331, 0, .meta.range={0,255,1} },
        { "On/Off", UI_PARAM_BOOL, 332, 0, .meta.en={LBL_BOOL,2}, .is_bitwise=0, .bit_mask=0 },
        { "-",      UI_PARAM_CONT, 0,   0, .meta.range={0,0,0} },
    } },
    EMPTY_PAGE
  }
};

/* ------------------------------ FX2 (DELAY) ----------------------------- */
const ui_menu_spec_t XVA1_MENU_FX2 = {
  .name = "FX2 Delay",
  .page_titles = { "Main","Tone","Time","Pan","-" },
  .pages = {
    { .params = {
        { "Mode",   UI_PARAM_ENUM, 340, 0, .meta.en={delayModeLabels,3} },
        { "Mix",    UI_PARAM_CONT, 341, 0, .meta.range={0,255,1} },
        { "Feedbk", UI_PARAM_CONT, 342, 0, .meta.range={0,255,1} },
        { "Width",  UI_PARAM_CONT, 343, 0, .meta.range={0,255,1} },
    } },
    { .params = {
        { "Tone", UI_PARAM_CONT, 344, 0, .meta.range={0,255,1} },
        { "HP",   UI_PARAM_CONT, 345, 0, .meta.range={0,255,1} },
        { "LP",   UI_PARAM_CONT, 346, 0, .meta.range={0,255,1} },
        { "-",    UI_PARAM_CONT, 0,   0, .meta.range={0,0,0} },
    } },
    { .params = {
        { "Time L", UI_PARAM_CONT, 347, 0, .meta.range={0,255,1} },
        { "Time R", UI_PARAM_CONT, 348, 0, .meta.range={0,255,1} },
        { "Offset", UI_PARAM_CONT, 349, 0, .meta.range={0,255,1} },
        { "Sync",   UI_PARAM_BOOL, 350, 0, .meta.en={LBL_BOOL,2}, .is_bitwise=0, .bit_mask=0 },
    } },
    { .params = {
        { "Pan",    UI_PARAM_CONT, 351, 0, .meta.range={0,255,1} },
        { "On/Off", UI_PARAM_BOOL, 352, 0, .meta.en={LBL_BOOL,2}, .is_bitwise=0, .bit_mask=0 },
        { "-",      UI_PARAM_CONT, 0,   0, .meta.range={0,0,0} },
        { "-",      UI_PARAM_CONT, 0,   0, .meta.range={0,0,0} },
    } },
    EMPTY_PAGE
  }
};

/* ------------------------------ FX3 (CHORUS) ---------------------------- */
const ui_menu_spec_t XVA1_MENU_FX3 = {
  .name = "FX3 Chorus",
  .page_titles = { "Main","Rate","Depth","Pan","-" },
  .pages = {
    { .params = {
        { "Mode", UI_PARAM_ENUM, 360, 0, .meta.en={chorusModeLabels,4} },
        { "Mix",  UI_PARAM_CONT, 361, 0, .meta.range={0,255,1} },
        { "Rate", UI_PARAM_CONT, 362, 0, .meta.range={0,255,1} },
        { "Depth",UI_PARAM_CONT, 363, 0, .meta.range={0,255,1} },
    } },
    { .params = {
        { "Tone", UI_PARAM_CONT, 364, 0, .meta.range={0,255,1} },
        { "LRPhas", UI_PARAM_ENUM, 365, 0, .meta.en={phaseLabels,4} },
        { "-",      UI_PARAM_CONT, 0,   0, .meta.range={0,0,0} },
        { "-",      UI_PARAM_CONT, 0,   0, .meta.range={0,0,0} },
    } },
    { .params = {
        { "Feedbk", UI_PARAM_CONT, 315, 0, .meta.range={0,255,1} },
        { "Dry",    UI_PARAM_CONT, 310, 0, .meta.range={0,255,1} },
        { "Wet",    UI_PARAM_CONT, 311, 0, .meta.range={0,255,1} },
        { "Mode",   UI_PARAM_ENUM, 312, 0, .meta.en={phaserModeLabels,3} },
    } },
    { .params = {
        { "Speed",  UI_PARAM_CONT, 314, 0, .meta.range={0,255,1} },
        { "Depth",  UI_PARAM_CONT, 313, 0, .meta.range={0,255,1} },
        { "On/Off", UI_PARAM_BOOL, 316, 0, .meta.en={LBL_BOOL,2}, .is_bitwise=0, .bit_mask=0 },
        { "-",      UI_PARAM_CONT, 0,   0, .meta.range={0,0,0} },
    } },
    EMPTY_PAGE
  }
};

/* ------------------------------ FX4 (REVERB) ---------------------------- */
const ui_menu_spec_t XVA1_MENU_FX4 = {
  .name = "FX4 Reverb",
  .page_titles = { "Main","Tone","Shape","Pan","-" },
  .pages = {
    { .params = {
        { "Mode", UI_PARAM_ENUM, 370, 0, .meta.en={reverbModeLabels,2} },
        { "Mix",  UI_PARAM_CONT, 371, 0, .meta.range={0,255,1} },
        { "Time", UI_PARAM_CONT, 372, 0, .meta.range={0,255,1} },
        { "Predly",UI_PARAM_CONT, 373, 0, .meta.range={0,255,1} },
    } },
    { .params = {
        { "Tone", UI_PARAM_CONT, 374, 0, .meta.range={0,255,1} },
        { "HP",   UI_PARAM_CONT, 375, 0, .meta.range={0,255,1} },
        { "LP",   UI_PARAM_CONT, 376, 0, .meta.range={0,255,1} },
        { "-",    UI_PARAM_CONT, 0,   0, .meta.range={0,0,0} },
    } },
    { .params = {
        { "Size", UI_PARAM_CONT, 377, 0, .meta.range={0,255,1} },
        { "Diff", UI_PARAM_CONT, 378, 0, .meta.range={0,255,1} },
        { "Dens", UI_PARAM_CONT, 379, 0, .meta.range={0,255,1} },
        { "Earl", UI_PARAM_CONT, 380, 0, .meta.range={0,255,1} },
    } },
    { .params = {
        { "Pan",    UI_PARAM_CONT, 381, 0, .meta.range={0,255,1} },
        { "Width",  UI_PARAM_CONT, 382, 0, .meta.range={0,255,1} },
        { "On/Off", UI_PARAM_BOOL, 383, 0, .meta.en={LBL_BOOL,2}, .is_bitwise=0, .bit_mask=0 },
        { "-",      UI_PARAM_CONT, 0,   0, .meta.range={0,0,0} },
    } },
    EMPTY_PAGE
  }
};

/* ============================== CART SPEC =============================== */
/**
 * @brief Spécification UI de la cartouche **XVA1**.
 * @ingroup cart
 *
 * @details
 * Déclare les 15 menus XVA1 et la configuration **déclarative** des cycles BM.
 * Ici, **BM8** (index 7) cycle entre les menus **FX1→FX2→FX3→FX4**.
 * - Pas de dépendance vers le contrôleur : données lues par l’UI côté `ui_controller`.
 * - `resume=true` : si on revient sur BM8, on reprend le dernier FX sélectionné.
 */
const ui_cart_spec_t CART_XVA1 = {
    .cart_name = "XVA1",
    .menus = {
        XVA1_MENU_OSC1,        /*  0 */
        XVA1_MENU_OSC2,        /*  1 */
        XVA1_MENU_OSC3,        /*  2 */
        XVA1_MENU_OSC4,        /*  3 */
        XVA1_MENU_FILTER,      /*  4 */
        XVA1_MENU_ENV_FILTER,  /*  5 */
        XVA1_MENU_ENV_AMP,     /*  6 */
        XVA1_MENU_ENV_PITCH,   /*  7 */
        XVA1_MENU_LFO12,       /*  8 */
        XVA1_MENU_LFO_MIDIMOD, /*  9 */
        XVA1_MENU_MIDI_GLOBAL, /* 10 */
        XVA1_MENU_FX1,         /* 11 */
        XVA1_MENU_FX2,         /* 12 */
        XVA1_MENU_FX3,         /* 13 */
        XVA1_MENU_FX4          /* 14 */
        /* (slot 15 inutilisé si UI_MENUS_PER_CART == 16) */
    },

    /* ------------------------------------------------------------------ */
    /* Cycles BM (data-only)                                              */
    /* ------------------------------------------------------------------ */
    .cycles = {
        /* BM1..BM7 : pas de cycle explicite (laisse à zéro) */
        [0] = { .count = 0 },
        [1] = { .count = 0 },
        [2] = { .count = 0 },
        [3] = { .count = 0 },
        [4] = { .count = 0 },
        [5] = { .count = 3, .idxs = { 5, 6, 7 }, .resume = true },
        [6] = { .count = 3, .idxs = { 8, 9, 10 }, .resume = true },
        /**
         * @brief BM8 → FX1→FX2→FX3→FX4
         * @details
         * Indices des menus FX dans `.menus[]` : 11, 12, 13, 14.
         * `resume=true` : revenir sur BM8 conserve l’option FX courante.
         */
        [7] = {
            .count  = 4,
            .idxs   = { 11, 12, 13, 14 },
            .resume = true
        },
    }
};
