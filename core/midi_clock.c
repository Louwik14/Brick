/**
 * @file midi_clock.c
 * @brief Générateur d’horloge MIDI (24 PPQN) basé sur GPT3.
 *
 * Ce module implémente la génération précise des **ticks MIDI Clock (F8)** :
 * - Utilise le timer matériel **TIM3** (via GPTD3) à 1 MHz.
 * - Envoie les messages `0xF8` à intervalles réguliers selon le BPM courant.
 * - Fournit un **callback applicatif** à chaque tick (pour séquenceur, clock_manager, etc.).
 * - Supporte start/stop dynamique et recalcul automatique à changement de tempo.
 *
 * @note Résolution : 1 µs (bpm min ≈ 38.2 à 24 PPQN).
 * @ingroup midi
 */

#include "ch.h"
#include "hal.h"
#include "midi_clock.h"
#include "midi.h"   /* pour midi_clock(MIDI_DEST_BOTH) */

/* ===== Sélection du timer =====
 * On utilise TIM3 (APB1, 16 bits) via GPTD3.
 */
#define MIDI_GPT_DRIVER       GPTD3

/* Base du timer après préscaler (Hz) :
 * 1 MHz → résolution 1 µs (bpm min ≈ 38.2 à 24 PPQN)
 */
#define MIDI_GPT_BASE_HZ      1000000U

/* === Callback tick (24 PPQN) === */
static midi_tick_cb_t s_tick_cb = NULL;

/* === Variables internes === */
static THD_WORKING_AREA(waMidiClk, 256);
static THD_FUNCTION(thMidiClk, arg);
static void gpt3_cb(GPTDriver *gptp);

static binary_semaphore_t clk_sem;
static float     s_bpm            = 120.0f;
static uint32_t  s_interval_ticks = 0;
static bool      s_running        = false;

/* === Configuration du GPT === */
static GPTConfig gpt3cfg = {
  .frequency = MIDI_GPT_BASE_HZ,
  .callback  = gpt3_cb,
  .cr2       = 0U,
  .dier      = 0U,
};

/**
 * @brief Calcule le nombre de ticks timer pour un BPM donné.
 */
static uint32_t compute_interval_ticks(float bpm) {
  if (bpm < 1.0f) bpm = 1.0f;
  float ticks_f = (60.0f * (float)MIDI_GPT_BASE_HZ) / (bpm * 24.0f);
  if (ticks_f < 1.0f)    ticks_f = 1.0f;
  if (ticks_f > 65535.f) ticks_f = 65535.f; /* TIM3 = 16 bits */
  return (uint32_t)(ticks_f + 0.5f);
}

/**
 * @brief Callback IRQ du timer GPT3 (tick MIDI 24 PPQN).
 */
static void gpt3_cb(GPTDriver *gptp) {
  (void)gptp;
  chSysLockFromISR();
  chBSemSignalI(&clk_sem);
  chSysUnlockFromISR();
}

/**
 * @brief Thread d’envoi des horloges MIDI (`0xF8`) et notification du callback.
 */
static THD_FUNCTION(thMidiClk, arg) {
  (void)arg;
#if CH_CFG_USE_REGISTRY
  chRegSetThreadName("midi_clk");
#endif
  while (true) {
    chBSemWait(&clk_sem);

    /* Envoi d’un message Clock MIDI (F8) vers USB + DIN */
    midi_clock(MIDI_DEST_BOTH);

    /* Notifie l’application (séquenceur, etc.) */
    if (s_tick_cb) s_tick_cb();
  }
}

/* ======================================================================
 *                              API PUBLIQUE
 * ====================================================================== */

/**
 * @brief Enregistre un callback appelé à chaque tick MIDI (24 PPQN).
 */
void midi_clock_register_tick_callback(midi_tick_cb_t cb) { s_tick_cb = cb; }

/**
 * @brief Initialise le générateur MIDI Clock (thread + GPT3).
 */
void midi_clock_init(void) {
  chBSemObjectInit(&clk_sem, true);
  (void)chBSemWaitTimeout(&clk_sem, TIME_IMMEDIATE); /* consomme le token initial */
  chThdCreateStatic(waMidiClk, sizeof(waMidiClk), NORMALPRIO + 3, thMidiClk, NULL);
  s_interval_ticks = compute_interval_ticks(s_bpm);
  gptStart(&MIDI_GPT_DRIVER, &gpt3cfg);
}

/**
 * @brief Démarre la génération de MIDI Clock.
 */
void midi_clock_start(void) {
  if (s_running) return;
  gptStartContinuous(&MIDI_GPT_DRIVER, s_interval_ticks);
  s_running = true;
}

/**
 * @brief Arrête la génération de MIDI Clock.
 */
void midi_clock_stop(void) {
  if (!s_running) return;
  gptStopTimer(&MIDI_GPT_DRIVER);
  s_running = false;
}

/**
 * @brief Modifie le tempo du générateur.
 * @param bpm Nouveau tempo en BPM.
 */
void midi_clock_set_bpm(float bpm) {
  s_bpm = bpm;
  s_interval_ticks = compute_interval_ticks(s_bpm);
  if (s_running) {
    gptStopTimer(&MIDI_GPT_DRIVER);
    gptStartContinuous(&MIDI_GPT_DRIVER, s_interval_ticks);
  }
}

/**
 * @brief Retourne le tempo actuel (BPM).
 */
float midi_clock_get_bpm(void) { return s_bpm; }

/**
 * @brief Indique si la clock MIDI est active.
 */
bool midi_clock_is_running(void) { return s_running; }
