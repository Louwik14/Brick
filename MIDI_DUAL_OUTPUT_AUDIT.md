# Audit MIDI Dual Output — Brick Firmware

## 1. Résumé exécutif
- **Conclusion** : le chemin `seq/keyboard` émet bien les NOTE ON/OFF en **double** (USB + DIN) via le hook `midi_tx3()` puis `midi_note_on/off(..., MIDI_DEST_BOTH, ...)`, lesquels routent explicitement vers les deux backends. Aucun trou identifié.
- **Chemins critiques** :
  - `apps/ui_keyboard_bridge.c` → `ui_backend_note_on/off()` → `midi_note_on/off(MIDI_DEST_BOTH, …)`.
  - `apps/seq_engine_runner.c` (runner séquenceur) → `apps/midi_helpers.h` → `apps/seq_midi_bridge.c` → `midi_note_on/off(MIDI_DEST_BOTH, …)`.
- **UART DIN** : initialisé systématiquement à 31250 bauds (`sdStart(SD2, …)`) lors du `midi_init()` lancé au boot.
- **STOP/CC#123 & NOTE_OFF** : STOP force `midi_all_notes_off()` sur tous les canaux, et `midi_note_on()` redirige les vélocités 0 vers `midi_note_off()`. Pas d’asymétrie détectée.

## 2. Carte des chemins d’émission
```
Clavier UI / Seq live
  ui_keyboard_bridge.c::sink_note_on/off
    → ui_backend_note_on/off()
      → midi_note_on/off(MIDI_DEST_BOTH,…)
        → midi_send()
          → send_uart()  (DIN @31250)
          → send_usb()   (USB MIDI EP2)

Séquenceur (Runner)
  seq_engine_runner.c::_runner_flush_queued_events()
    → apps/midi_helpers.h::midi_note_on/off()
      → midi_tx3(b0,b1,b2) fort dans seq_midi_bridge.c
        → midi_note_on/off(MIDI_DEST_BOTH,…)
          → midi_send() → send_uart() + send_usb()
```

## 3. Détection code & artefacts
- **NOTE ON/OFF clavier** : `ui_backend_note_on/off()` appellent `midi_note_on/off(MIDI_DEST_BOTH, …)`【F:ui/ui_backend.c†L933-L944】.
- **Hook séquenceur** : `apps/midi_helpers.h` encode en 3 octets, `midi_tx3()` fort du firmware relaye vers `midi_note_on/off(MIDI_DEST_BOTH, …)`【F:apps/midi_helpers.h†L22-L36】【F:apps/seq_midi_bridge.c†L5-L32】.
- **Runner** : `_runner_flush_queued_events()` alimente les helpers NOTE ON/OFF; STOP diffuse CC#123 via `midi_all_notes_off()`【F:apps/seq_engine_runner.c†L141-L162】【F:apps/seq_engine_runner.c†L520-L533】.
- **Backend clavier** : `_direct_note_on/off()` → `ui_backend_note_on/off()` assurant la capture ARP + enregistrement + sortie MIDI【F:apps/ui_keyboard_bridge.c†L68-L133】.
- **Routage central** : `midi_send()` duplique vers UART et USB pour `MIDI_DEST_BOTH`; `midi_note_on/off()` appellent `midi_send()`【F:midi/midi.c†L368-L390】.
- **Initialisation UART DIN** : `midi_init()` configure SD2 @31250 et lance le thread USB TX【F:midi/midi.c†L205-L222】.
- **USB MIDI** : `send_usb()` packe les messages vers EP2 et `post_mb_or_drop()` (mailbox)【F:midi/midi.c†L274-L356】.
- **Configuration HAL** : USART2 activé (`STM32_SERIAL_USE_USART2=TRUE`) et pins PA2/PA3 en Alternate 7 dans la board config, assurant le DIN【F:cfg/mcuconf.h†L271-L274】【F:board/board.h†L350-L417】.
- **Initialisation système** : `main.c::io_realtime_init()` appelle `usb_device_start()` puis `midi_init()` dès le boot【F:main.c†L70-L120】.

## 4. Analyse détaillée
- **Symétrie NOTE ON/OFF** :
  - `_runner_flush_queued_events()` traite OFF puis ON en utilisant les helpers partagés, garantissant la symétrie sur les deux ports【F:apps/seq_engine_runner.c†L520-L533】.
  - `midi_note_on()` renvoie automatiquement vers `midi_note_off()` si la vélocité vaut 0, évitant tout drop【F:midi/midi.c†L381-L389】.
- **STOP → CC#123** : le runner boucle `midi_all_notes_off()` (CC#123) sur 16 canaux lors d’un STOP, via la même chaîne `midi_tx3`→`midi_note_off`→`midi_send`→USB+DIN【F:apps/seq_engine_runner.c†L141-L162】【F:apps/midi_helpers.h†L33-L36】.
- **Hot/Cold** :
  - Les helpers `apps/midi_helpers.h` sont Reader-only et ne manipulent pas la façade cold; `ui_backend_note_on/off()` réside côté UI (cold) mais délègue à `midi.c` (driver chaud) déjà mutualisé. Pas de violation `no-cold-in-tick` observée.
  - `midi_send()` encapsule le routage commun sans sections particulières, évitant la duplication hot/cold.
- **Transport/Router** : `midi_dest_t` expose explicitement `MIDI_DEST_BOTH`; aucun flag compile-time ne désactive DIN par défaut【F:midi/midi.h†L60-L114】.

## 5. Recommandations
- **Validation terrain** : brancher un analyseur logique sur PA2 (USART2_TX) et un host USB pour vérifier la trace simultanée. Utiliser le patch de trace ci-dessous pour instrumenter côté firmware.
- **Diagnostic continu** : conserver `midi_probe` pour surveiller NOTE_OFF; compléter par un compteur UART (ex: watermark `sdGetTransmitBufferSpace()`) si des doutes de saturation apparaissent.
- **Pas de changement requis** : le double routage est déjà effectif et inconditionnel. Simplement documenter cette chaîne pour l’équipe QA.

## 6. Patch de trace optionnel (`#if DEBUG_MIDI_TRACE`)
À insérer dans `midi.c` juste après le `switch` de `midi_send()` (avant `break`), afin de logguer chaque message vers les deux ports :

```c
#if DEBUG_MIDI_TRACE
#include "chsys.h"
static void _midi_trace_log(const char *port, const uint8_t *msg, size_t n) {
    chSysLock();
    chprintf((BaseSequentialStream *)&SDU1,
             "[MIDI][%s] t=%lu st=%02X d1=%02X d2=%02X\r\n",
             port,
             (unsigned long)chVTGetSystemTimeX(),
             msg[0], n > 1 ? msg[1] : 0, n > 2 ? msg[2] : 0);
    chSysUnlock();
}
#endif

static void midi_send(midi_dest_t d, const uint8_t *m, size_t n) {
    switch (d) {
      case MIDI_DEST_UART:
        send_uart(m, n);
#if DEBUG_MIDI_TRACE
        _midi_trace_log("DIN", m, n);
#endif
        break;
      case MIDI_DEST_USB:
        send_usb(m, n);
#if DEBUG_MIDI_TRACE
        _midi_trace_log("USB", m, n);
#endif
        break;
      case MIDI_DEST_BOTH:
        send_uart(m, n);
        send_usb(m, n);
#if DEBUG_MIDI_TRACE
        _midi_trace_log("DIN", m, n);
        _midi_trace_log("USB", m, n);
#endif
        break;
      default:
        break;
    }
}
```

> **Test terrain** : activer `DEBUG_MIDI_TRACE`, appuyer sur une touche → observer deux lignes (DIN & USB) pour la même note/vel dans le terminal (SDU1).

## 7. Annexe — Commandes de vérification (Windows `findstr`)
```
findstr /S /N /I /C:"note_on" /C:"note_off" apps\* ui\* midi\*
findstr /S /N /I /C:"midi_note_on" /C:"MIDI_DEST_BOTH" ui\* apps\* midi\*
findstr /S /N /I /C:"midi_tx3" /C:"seq_midi" apps\*
findstr /S /N /I /C:"sdStart" /C:"31250" midi\*
findstr /S /N /I /C:"USART2" board\* cfg\*
findstr /S /N /I /C:"midi_init" main.c
```
