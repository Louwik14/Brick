/**
 * @file drv_buttons.c
 * @brief Driver matériel pour la lecture des boutons via registres à décalage **74HC165**.
 * @details
 * Ce module gère la capture, le filtrage et la détection d’événements des boutons
 * physiques de la surface Brick.
 *
 * Les 74HC165 sont lus en série à l’aide de lignes :
 * - **LOAD** (latch parallèle),
 * - **CLK** (décalage série),
 * - **DATA** (sortie série des registres chainés).
 *
 * Le driver :
 * - effectue un scan périodique (~200 Hz),
 * - détecte automatiquement les transitions (pression / relâchement),
 * - publie les événements dans une **mailbox** pour consommation asynchrone,
 * - offre un accès instantané à l’état courant de chaque bouton.
 *
 * @ingroup drivers
 */

#include "drv_buttons.h"
#include "ch.h"
#include "hal.h"
#include "brick_config.h"

/* ====================================================================== */
/*                        CONFIGURATION MATÉRIELLE                        */
/* ====================================================================== */

/** @brief Ligne LOAD des registres 74HC165 (Latch des entrées). */
#define SR_LOAD_LINE    PAL_LINE(GPIOB, 0)
/** @brief Ligne CLOCK pour décalage série. */
#define SR_CLK_LINE     PAL_LINE(GPIOB, 1)
/** @brief Ligne DATA (sortie série des registres). */
#define SR_DATA_LINE    PAL_LINE(GPIOG, 11)

/* ====================================================================== */
/*                           ÉTATS INTERNES                               */
/* ====================================================================== */

/** @brief États instantanés des boutons (true = pressé). */
static bool button_states[NUM_BUTTONS];
/** @brief États précédents des boutons (pour détection d’événements). */
static bool last_states[NUM_BUTTONS];

/* ====================================================================== */
/*                     FILE D’ÉVÉNEMENTS (MAILBOX)                        */
/* ====================================================================== */

/** @brief Mailbox utilisée pour poster les événements boutons. */
static mailbox_t evt_mb;
/** @brief File de messages associée à la mailbox. */
static CCM_DATA msg_t evt_queue[DRV_BUTTONS_QUEUE_LEN];

#if defined(BRICK_ENABLE_INSTRUMENTATION)
static uint16_t s_evt_fill = 0;
static uint16_t s_evt_high_water = 0;
static uint32_t s_evt_drop_count = 0;
#endif

/* ====================================================================== */
/*                     LECTURE DES REGISTRES À DÉCALAGE                   */
/* ====================================================================== */

/**
 * @brief Lit l’état courant des boutons via les registres 74HC165.
 * @details
 * Chaque bit est lu séquentiellement et comparé à l’état précédent
 * pour générer des événements `PRESS` ou `RELEASE` envoyés en mailbox.
 */
static void sr_read_buttons(void) {
    // Latch des entrées
    palClearLine(SR_LOAD_LINE);
    chThdSleepMicroseconds(1);
    palSetLine(SR_LOAD_LINE);

    for (int i = 0; i < NUM_BUTTONS; i++) {
        bool bit = !palReadLine(SR_DATA_LINE); // inversion car logique active bas
        button_states[i] = bit;

        // Détection de transition
        if (bit != last_states[i]) {
            button_event_t evt;
            evt.id = i;
            evt.type = bit ? BUTTON_EVENT_PRESS : BUTTON_EVENT_RELEASE;

            // Encodage compact : 8 bits d’ID + 8 bits de type
#if defined(BRICK_ENABLE_INSTRUMENTATION)
            const msg_t post_res = chMBPostTimeout(&evt_mb,
                                                  (msg_t)(evt.id | (evt.type << 8)),
                                                  TIME_IMMEDIATE);
            if (post_res == MSG_OK) {
                osalSysLock();
                if (s_evt_fill < DRV_BUTTONS_QUEUE_LEN) {
                    s_evt_fill++;
                    if (s_evt_fill > s_evt_high_water) {
                        s_evt_high_water = s_evt_fill;
                    }
                }
                osalSysUnlock();
            } else {
                osalSysLock();
                s_evt_drop_count++;
                osalSysUnlock();
            }
#else
            (void)chMBPostTimeout(&evt_mb, (msg_t)(evt.id | (evt.type << 8)), TIME_IMMEDIATE);
#endif
        }

        last_states[i] = bit;

        // Pulse clock pour passer au bit suivant
        palSetLine(SR_CLK_LINE);
        chThdSleepMicroseconds(1);
        palClearLine(SR_CLK_LINE);
    }
}

/* ====================================================================== */
/*                           THREAD DE SCAN                               */
/* ====================================================================== */

/** @brief Thread responsable du scan périodique des boutons (~200 Hz). */
static CCM_DATA THD_WORKING_AREA(waButtons, 2048);
static THD_FUNCTION(ButtonsThread, arg) {
    (void)arg;
    chRegSetThreadName("Buttons");

    while (true) {
        sr_read_buttons();
        chThdSleepMilliseconds(5);  // ≈ 200 Hz
    }
}

/* ====================================================================== */
/*                          API PUBLIQUE                                  */
/* ====================================================================== */

/**
 * @brief Initialise le driver boutons et démarre le thread de scan.
 *
 * Configure les lignes GPIO, initialise la mailbox d’événements
 * et lance le thread périodique de lecture.
 */
void drv_buttons_start(void) {
    palSetLineMode(SR_LOAD_LINE, PAL_MODE_OUTPUT_PUSHPULL);
    palSetLineMode(SR_CLK_LINE, PAL_MODE_OUTPUT_PUSHPULL);
    palSetLineMode(SR_DATA_LINE, PAL_MODE_INPUT_PULLUP);

    palSetLine(SR_LOAD_LINE);
    palClearLine(SR_CLK_LINE);

    // Init états
    for (int i = 0; i < NUM_BUTTONS; i++) {
        button_states[i] = false;
        last_states[i] = false;
    }

    chMBObjectInit(&evt_mb, evt_queue, DRV_BUTTONS_QUEUE_LEN);
#if defined(BRICK_ENABLE_INSTRUMENTATION)
    s_evt_fill = 0;
    s_evt_high_water = 0;
    s_evt_drop_count = 0;
#endif
    chThdCreateStatic(waButtons, sizeof(waButtons), NORMALPRIO, ButtonsThread, NULL);
}

/**
 * @brief Vérifie l’état courant d’un bouton.
 * @param id Identifiant du bouton (0 à NUM_BUTTONS-1)
 * @return `true` si le bouton est pressé, sinon `false`.
 */
bool drv_button_is_pressed(int id) {
    if (id < 0 || id >= NUM_BUTTONS) return false;
    return button_states[id];
}

/**
 * @brief Récupère un événement de bouton depuis la mailbox.
 *
 * @param[out] evt  Structure où stocker l’événement lu.
 * @param[in] timeout  Délai maximum d’attente (ex. `TIME_IMMEDIATE` ou `TIME_INFINITE`).
 * @return `true` si un événement a été lu, sinon `false` (timeout).
 */
bool drv_buttons_poll(button_event_t *evt, systime_t timeout) {
    msg_t msg;
    if (chMBFetchTimeout(&evt_mb, &msg, timeout) == MSG_OK) {
#if defined(BRICK_ENABLE_INSTRUMENTATION)
        osalSysLock();
        if (s_evt_fill > 0U) {
            s_evt_fill--;
        }
        osalSysUnlock();
#endif
        evt->id   = msg & 0xFF;
        evt->type = (msg >> 8) & 0xFF;
        return true;
    }
    return false;
}

#if defined(BRICK_ENABLE_INSTRUMENTATION)
uint16_t drv_buttons_queue_high_water(void) {
    osalSysLock();
    const uint16_t high = s_evt_high_water;
    osalSysUnlock();
    return high;
}

uint32_t drv_buttons_queue_drop_count(void) {
    osalSysLock();
    const uint32_t drops = s_evt_drop_count;
    osalSysUnlock();
    return drops;
}

uint16_t drv_buttons_queue_fill(void) {
    osalSysLock();
    const uint16_t fill = s_evt_fill;
    osalSysUnlock();
    return fill;
}

void drv_buttons_stats_reset(void) {
    osalSysLock();
    s_evt_fill = 0;
    s_evt_high_water = 0;
    s_evt_drop_count = 0;
    osalSysUnlock();
}
#endif
