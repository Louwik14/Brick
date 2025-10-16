/**
 * @file midi.h
 * @brief Interface du module MIDI (UART + USB) pour ChibiOS.
 *
 * Fournit une API unifiée pour l’envoi de messages MIDI sur ports :
 * - **UART DIN (31250 bauds)**
 * - **USB MIDI Class Compliant**
 *
 * Fonctions principales :
 * - Envoi des messages MIDI standards (Note On/Off, CC, Program Change, etc.)
 * - Gestion des messages “System Common” et “System Realtime”
 * - Statistiques de transmission détaillées
 * - Routage entre plusieurs destinations : UART, USB, ou les deux
 *
 * @note L’implémentation est dans `midi.c`
 * @ingroup drivers
 */

#ifndef MIDI_H
#define MIDI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ====================================================================== */
/*                        CONFIGURATION GLOBALE                           */
/* ====================================================================== */

/**
 * @brief Comportement en cas de débordement de la file d’attente USB MIDI.
 *
 * Si défini à 1, le message le plus ancien est supprimé pour insérer le nouveau.
 * Sinon, le nouveau message est perdu.
 */
#ifndef MIDI_MB_DROP_OLDEST
#define MIDI_MB_DROP_OLDEST  0
#endif

/**
 * @brief Délai (en µs) avant de relâcher le sémaphore d’envoi pour les messages temps réel.
 */
#ifndef MIDI_RT_MICROWAIT_US
#define MIDI_RT_MICROWAIT_US 50
#endif

/**
 * @brief Numéro de câble USB MIDI (0 pour unique interface).
 */
#ifndef MIDI_USB_CABLE
#define MIDI_USB_CABLE  0u
#endif

/* ====================================================================== */
/*                              TYPES ET STRUCTURES                       */
/* ====================================================================== */

/**
 * @enum midi_dest_t
 * @brief Sélectionne la ou les destinations d’envoi MIDI.
 */
typedef enum {
  MIDI_DEST_NONE = 0,  /**< Aucune sortie */
  MIDI_DEST_UART,      /**< Envoi uniquement via port UART DIN */
  MIDI_DEST_USB,       /**< Envoi uniquement via USB MIDI */
  MIDI_DEST_BOTH       /**< Envoi sur les deux sorties */
} midi_dest_t;

/**
 * @struct midi_tx_stats_t
 * @brief Statistiques de transmission MIDI (pour diagnostic et debug).
 */
typedef struct {
  volatile uint32_t tx_sent_immediate;      /**< Messages envoyés immédiatement (EP libre) */
  volatile uint32_t tx_sent_batched;        /**< Messages regroupés et envoyés par lot */
  volatile uint32_t rt_f8_drops;            /**< Messages Clock (0xF8) perdus faute de place */
  volatile uint32_t rt_f8_burst_sent;       /**< Groupes de messages Clock envoyés en rafale */
  volatile uint32_t rt_other_enq_fallback;  /**< Autres messages temps réel mis en file (fallback) */
  volatile uint32_t tx_mb_drops;            /**< Messages perdus (mailbox pleine) */
  volatile uint32_t usb_not_ready_drops;    /**< Messages perdus (USB non prêt) */
} midi_tx_stats_t;

/** @brief Statistiques globales d’état et de performance MIDI. */
extern midi_tx_stats_t midi_tx_stats;

/* ====================================================================== */
/*                              INITIALISATION                            */
/* ====================================================================== */

/**
 * @brief Initialise le module MIDI (UART + thread TX USB).
 *
 * Configure le port UART DIN à 31250 bauds, initialise la mailbox et crée
 * le thread responsable de l’envoi USB.
 */
void midi_init(void);

/* ====================================================================== */
/*                        COMMANDES “CHANNEL VOICE”                       */
/* ====================================================================== */

/**
 * @brief Envoie une note ON.
 * @param dest Destination d’envoi (UART/USB/BOTH)
 * @param ch Canal MIDI [0–15]
 * @param note Numéro de note [0–127]
 * @param vel Vélocité [0–127] (0 = Note Off)
 */
void midi_note_on(midi_dest_t dest, uint8_t ch, uint8_t note, uint8_t vel);

/**
 * @brief Envoie une note OFF.
 */
void midi_note_off(midi_dest_t dest, uint8_t ch, uint8_t note, uint8_t vel);

/**
 * @brief Envoie un message de pression polyphonique (aftertouch).
 */
void midi_poly_aftertouch(midi_dest_t dest, uint8_t ch, uint8_t note, uint8_t pressure);

/**
 * @brief Envoie un message de changement de contrôle (Control Change).
 */
void midi_cc(midi_dest_t dest, uint8_t ch, uint8_t cc, uint8_t val);

/**
 * @brief Envoie un changement de programme (Program Change).
 */
void midi_program_change(midi_dest_t dest, uint8_t ch, uint8_t program);

/**
 * @brief Envoie une pression de canal (Channel Pressure).
 */
void midi_channel_pressure(midi_dest_t dest, uint8_t ch, uint8_t pressure);

/**
 * @brief Envoie un pitch bend 14 bits.
 * @param value14b Valeur entre -8192 et +8191
 */
void midi_pitchbend(midi_dest_t dest, uint8_t ch, int16_t value14b);

/* ====================================================================== */
/*                      COMMANDES “SYSTEM COMMON”                         */
/* ====================================================================== */

void midi_mtc_quarter_frame(midi_dest_t dest, uint8_t qf);
void midi_song_position(midi_dest_t dest, uint16_t pos14);
void midi_song_select(midi_dest_t dest, uint8_t song);
void midi_tune_request(midi_dest_t dest);

/* ====================================================================== */
/*                      COMMANDES “SYSTEM REALTIME”                       */
/* ====================================================================== */

void midi_clock(midi_dest_t dest);
void midi_start(midi_dest_t dest);
void midi_continue(midi_dest_t dest);
void midi_stop(midi_dest_t dest);
void midi_active_sensing(midi_dest_t dest);
void midi_system_reset(midi_dest_t dest);

/* ====================================================================== */
/*                       MESSAGES DE MODE DE CANAL                        */
/* ====================================================================== */

void midi_all_sound_off(midi_dest_t dest, uint8_t ch);
void midi_reset_all_controllers(midi_dest_t dest, uint8_t ch);
void midi_local_control(midi_dest_t dest, uint8_t ch, bool on);
void midi_all_notes_off(midi_dest_t dest, uint8_t ch);
void midi_omni_mode_off(midi_dest_t dest, uint8_t ch);
void midi_omni_mode_on(midi_dest_t dest, uint8_t ch);
void midi_mono_mode_on(midi_dest_t dest, uint8_t ch, uint8_t num_channels);
void midi_poly_mode_on(midi_dest_t dest, uint8_t ch);

/* ====================================================================== */
/*                              OUTILS                                    */
/* ====================================================================== */

/**
 * @brief Réinitialise les compteurs de statistiques MIDI.
 */
void midi_stats_reset(void);

/** @brief Retourne le plus haut niveau de remplissage observé sur la mailbox USB. */
uint16_t midi_usb_queue_high_watermark(void);

#endif /* MIDI_H */
