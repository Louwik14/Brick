#include "ui_arp_menu.h"
#include <string.h>

// --- ARP: Spécification UI du sous-menu Arpégiateur Keyboard ---

static const char* const s_onoff_labels[]       = { "Off", "On" };
static const char* const s_rate_labels[]        = { "1/4", "1/8", "1/16", "1/32", "Tri" };
static const char* const s_pattern_labels[]     = { "Up", "Down", "UpDn", "Rnd", "Chord" };
static const char* const s_accent_labels[]      = { "Off", "1st", "Alt", "Rnd" };
static const char* const s_strum_labels[]       = { "Off", "Up", "Down", "Alt", "Rnd" };
static const char* const s_trigger_labels[]     = { "Hold", "Retrig", "Free" };
static const char* const s_direction_labels[]   = { "Normal", "PingPong", "RndWalk" };
static const char* const s_lfo_target_labels[]  = { "Gate", "Vel", "Pitch" };
static const char* const s_sync_mode_labels[]   = { "Int", "Clock", "Free" };

// --- ARP: Page 1 Core ---
static const ui_page_spec_t s_page_core = {
  .params = {
    { .label="On",    .kind=UI_PARAM_ENUM, .dest_id=KBD_ARP_UI_ID(KBD_ARP_LOCAL_ONOFF),
      .default_value=0, .meta.en={ .labels=s_onoff_labels, .count=2 }, .is_bitwise=false, .bit_mask=0 },
    { .label="Rate",  .kind=UI_PARAM_ENUM, .dest_id=KBD_ARP_UI_ID(KBD_ARP_LOCAL_RATE),
      .default_value=2, .meta.en={ .labels=s_rate_labels, .count=5 }, .is_bitwise=false, .bit_mask=0 },
    { .label="Oct",   .kind=UI_PARAM_CONT, .dest_id=KBD_ARP_UI_ID(KBD_ARP_LOCAL_OCT_RANGE),
      .default_value=1, .meta.range={ .min=1, .max=4, .step=1 }, .is_bitwise=false, .bit_mask=0 },
    { .label="Pattern", .kind=UI_PARAM_ENUM, .dest_id=KBD_ARP_UI_ID(KBD_ARP_LOCAL_PATTERN),
      .default_value=0, .meta.en={ .labels=s_pattern_labels, .count=5 }, .is_bitwise=false, .bit_mask=0 }
  },
  .header_label = "Core"
};

// --- ARP: Page 2 Groove ---
static const ui_page_spec_t s_page_groove = {
  .params = {
    { .label="Gate", .kind=UI_PARAM_CONT, .dest_id=KBD_ARP_UI_ID(KBD_ARP_LOCAL_GATE),
      .default_value=60, .meta.range={ .min=10, .max=100, .step=5 }, .is_bitwise=false, .bit_mask=0 },
    { .label="Swing", .kind=UI_PARAM_CONT, .dest_id=KBD_ARP_UI_ID(KBD_ARP_LOCAL_SWING),
      .default_value=0, .meta.range={ .min=0, .max=75, .step=5 }, .is_bitwise=false, .bit_mask=0 },
    { .label="Accent", .kind=UI_PARAM_ENUM, .dest_id=KBD_ARP_UI_ID(KBD_ARP_LOCAL_ACCENT),
      .default_value=0, .meta.en={ .labels=s_accent_labels, .count=4 }, .is_bitwise=false, .bit_mask=0 },
    { .label="VelRnd", .kind=UI_PARAM_CONT, .dest_id=KBD_ARP_UI_ID(KBD_ARP_LOCAL_VEL_RAND),
      .default_value=0, .meta.range={ .min=0, .max=20, .step=1 }, .is_bitwise=false, .bit_mask=0 }
  },
  .header_label = "Groove"
};

// --- ARP: Page 3 Strum ---
static const ui_page_spec_t s_page_strum = {
  .params = {
    { .label="Strum", .kind=UI_PARAM_ENUM, .dest_id=KBD_ARP_UI_ID(KBD_ARP_LOCAL_STRUM_MODE),
      .default_value=0, .meta.en={ .labels=s_strum_labels, .count=5 }, .is_bitwise=false, .bit_mask=0 },
    { .label="Offset", .kind=UI_PARAM_CONT, .dest_id=KBD_ARP_UI_ID(KBD_ARP_LOCAL_STRUM_OFFSET),
      .default_value=0, .meta.range={ .min=0, .max=30, .step=1 }, .is_bitwise=false, .bit_mask=0 },
    { .label="Repeat", .kind=UI_PARAM_CONT, .dest_id=KBD_ARP_UI_ID(KBD_ARP_LOCAL_REPEAT),
      .default_value=1, .meta.range={ .min=1, .max=4, .step=1 }, .is_bitwise=false, .bit_mask=0 },
    { .label="Trig", .kind=UI_PARAM_ENUM, .dest_id=KBD_ARP_UI_ID(KBD_ARP_LOCAL_TRIGGER),
      .default_value=0, .meta.en={ .labels=s_trigger_labels, .count=3 }, .is_bitwise=false, .bit_mask=0 }
  },
  .header_label = "Strum"
};

// --- ARP: Page 4 Pitch ---
static const ui_page_spec_t s_page_pitch = {
  .params = {
    { .label="Trans", .kind=UI_PARAM_CONT, .dest_id=KBD_ARP_UI_ID(KBD_ARP_LOCAL_TRANSPOSE),
      .default_value=0, .meta.range={ .min=-12, .max=12, .step=1 }, .is_bitwise=false, .bit_mask=0 },
    { .label="Spread", .kind=UI_PARAM_CONT, .dest_id=KBD_ARP_UI_ID(KBD_ARP_LOCAL_SPREAD),
      .default_value=0, .meta.range={ .min=0, .max=100, .step=5 }, .is_bitwise=false, .bit_mask=0 },
    { .label="OctSh", .kind=UI_PARAM_CONT, .dest_id=KBD_ARP_UI_ID(KBD_ARP_LOCAL_OCT_SHIFT),
      .default_value=0, .meta.range={ .min=-1, .max=1, .step=1 }, .is_bitwise=false, .bit_mask=0 },
    { .label="Dir", .kind=UI_PARAM_ENUM, .dest_id=KBD_ARP_UI_ID(KBD_ARP_LOCAL_DIRECTION_BEHAV),
      .default_value=0, .meta.en={ .labels=s_direction_labels, .count=3 }, .is_bitwise=false, .bit_mask=0 }
  },
  .header_label = "Pitch"
};

// --- ARP: Page 5 Creative ---
static const ui_page_spec_t s_page_creative = {
  .params = {
    { .label="Pattern", .kind=UI_PARAM_CONT, .dest_id=KBD_ARP_UI_ID(KBD_ARP_LOCAL_PATTERN_SELECT),
      .default_value=1, .meta.range={ .min=1, .max=8, .step=1 }, .is_bitwise=false, .bit_mask=0 },
    { .label="Morph", .kind=UI_PARAM_CONT, .dest_id=KBD_ARP_UI_ID(KBD_ARP_LOCAL_PATTERN_MORPH),
      .default_value=0, .meta.range={ .min=0, .max=100, .step=5 }, .is_bitwise=false, .bit_mask=0 },
    { .label="LFO", .kind=UI_PARAM_ENUM, .dest_id=KBD_ARP_UI_ID(KBD_ARP_LOCAL_LFO_TARGET),
      .default_value=0, .meta.en={ .labels=s_lfo_target_labels, .count=3 }, .is_bitwise=false, .bit_mask=0 },
    { .label="Sync", .kind=UI_PARAM_ENUM, .dest_id=KBD_ARP_UI_ID(KBD_ARP_LOCAL_SYNC_MODE),
      .default_value=0, .meta.en={ .labels=s_sync_mode_labels, .count=3 }, .is_bitwise=false, .bit_mask=0 }
  },
  .header_label = "Creative"
};

// --- ARP: Menu unique ---
static const ui_menu_spec_t s_menu_arp = {
  .name = "ARPEGIATOR",
  .page_titles = { "Core", "Groove", "Strum", "Pitch", "Creative" },
  .pages = { &s_page_core, &s_page_groove, &s_page_strum, &s_page_pitch, &s_page_creative }
};

// --- ARP: Cartouche exportée ---
const ui_cart_spec_t ui_keyboard_arp_menu_spec = {
  .cart_name = "",
  .menus = { &s_menu_arp },
  .cycles = {
    [0] = { .count=0 }, [1] = { .count=0 }, [2] = { .count=0 }, [3] = { .count=0 },
    [4] = { .count=0 }, [5] = { .count=0 }, [6] = { .count=0 }, [7] = { .count=0 }
  }
};
