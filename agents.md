# 🧑‍💻 Agent Codex — Brick / SEQ Runtime

> **Rôle**  
> Agent de génération de code C/C++ temps réel pour **Brick** (ChibiOS 21.11.x, STM32F429).  
> Objectif : **produire et intégrer** le moteur de séquenceur réel (polyphonique, p-locks, micro-timing) dans l’architecture Brick **sans violer les invariants** (séparation UI / Core / Bus, Doxygen complet, pas de dépendances circulaires).

---

## 🎯 Cible de livraison

1) **Modules complets (compilables, documentés Doxygen)** :
- `seq_model.c/.h` — structures pures : pattern 64×4, steps, p-locks, offsets, quantize, transpose, scale.
- `seq_engine.c/.h` — reader (callback clock), scheduler, player MIDI, P-locks CART, poly/retrigger safe.
- `seq_runtime.c/.h` — **snapshot immuable** publié par l’engine ; getters sûrs pour l’UI.
- `seq_led_bridge.c/.h` — pont thread-safe pour le renderer LEDs (page visible, held mask, marquage params).
- *(Optionnel)* `seq_live_capture.c/.h` — façade d’enregistrement live (quantize+scale).

2) **Intégration UI** (sans couplage direct à la clock/driver) :
- `ui_model_param_display_value_slot()` : remplacer le mock preview par la lecture du snapshot `seq_runtime_*`.
- `ui_shortcuts` : en “hold”, consommer l’encodeur et, si fourni, appeler `seq_engine_apply_plock_delta(...)`.
- `ui_led_seq` : lecture via `seq_led_bridge` uniquement (aucun accès direct à la clock).

3) **Docs & checks** :
- **Doxygen complet** (`@file`, `@ingroup`, `@brief`, `@details`) ; groupes : `seq_model`, `seq_engine`, `seq_runtime`, `seq_led_bridge`.
- **Checklist DoD** (en fin de fichier).

---

## 🧩 Invariants & architecture Brick (rappel obligatoire)

- **Séparation stricte** : UI → `ui_backend` ; pas d’UART/bus depuis l’UI. Le bus cart reste confiné à `cart_bus/cart_link`.
- **Renderer pur** : `ui_renderer` et `ui_led_seq` ne tirent que l’état UI/bridges ; aucun accès `clock_manager` direct.
- **Threading** : `thMidiClk (N+3)`, `cart_tx_thread (N+2)`, `thSeqPlayer (N+1)`, `UIThread (N)` ; ISR ultra-courtes, pas de blocage en temps critique.
- **NOTE_OFF jamais droppé** ; LEDs non bloquantes ; zéro dépendance circulaire.
- **Shadow register** côté link/cart, **UI sans I/O** ; `ui_backend` est l’unique pont neutre UI→bas.

---

## 📦 Spécifications fonctionnelles SEQ (résumé)

- 64 steps, **4 voix** indépendantes (longueurs distinctes → **polyrhythmie**).
- **P-Locks** par step (INTERNAL) ; param-only = **bleu**, steps actifs = **vert**, playhead = **blanc** (priorité : blanc > bleu > vert > off).
- **Offsets globaux** (page ALL) : `Transp`, `Vel`, `Len`, `Mic` en **post-process** (n’écrasent pas les valeurs par voix).
- **REC live** : distribution dynamique des notes entrantes sur V1..V4 (grave→aigu, rotation si saturé). **NOTE_OFF jamais perdu**.
- **Snapshot immuable** exposé à l’UI (`seq_runtime_t`) : lecture lock-free depuis l’UI.

---

## 🔌 Interfaces à produire

### `seq_model.h` (pur, sans I/O)
- Types : `seq_step_t`, `seq_voice_t`, `seq_pattern_t`, `seq_plock_mask_t`, `seq_offsets_t`, ranges/scale/quantize.
- API : init/clear, set/get, toggle, gestion p-locks, micro-timing, versioning `gen`.

### `seq_engine.h`
- Init/start/stop ; enregistrement de callback clock : `clock_manager_register_step_callback2(...)`.
- API P-Locks :  
  `void seq_engine_apply_plock_delta(uint8_t param_slot, int16_t delta, uint64_t held_mask);`
- Émission : `midi_note_on/off`, `cart_link_param_changed` (p-locks CART) ; **poly/retrigger safe via `on_id`**.

### `seq_runtime.h`
- `const seq_runtime_t* seq_runtime_get_snapshot(void);`
- `bool    seq_runtime_step_has_note(const seq_runtime_t*, uint32_t step);`
- `bool    seq_runtime_step_param_is_plocked(const seq_runtime_t*, uint32_t step, uint8_t param);`
- `int16_t seq_runtime_step_param_value(const seq_runtime_t*, uint32_t step, uint8_t param);`
- `uint32_t seq_runtime_playhead_index(const seq_runtime_t*);`

### `seq_led_bridge.h`
- Accès thread-safe au **snapshot** + **page visible** + **held mask** + **slots params (0..3)**.  
- Fournit les couleurs au renderer SEQ, **sans clock directe** (via `ui_led_backend`).

### *(Optionnel) `seq_live_capture.h`*
```c
typedef struct { uint8_t note, vel, ch, track; } seq_live_note_t;
void seq_request_record(bool on);
void seq_live_capture_note_on(const seq_live_note_t* e);
void seq_live_capture_note_off(const seq_live_note_t* e);
```

---

## 🔧 Règles d’intégration UI (extraits à appliquer)

- **Preview param en hold** :
  ```c
  if (ui_model_seq_preview_active() && ui_model_seq_preview_param_is_marked(slot)) {
      const seq_runtime_t* rt = seq_runtime_get_snapshot();
      return seq_runtime_step_param_value(rt, step, param);
  }
  return live;
  ```
- **Hold/encodeur** : consommer l’événement côté `ui_shortcuts` ; appeler `seq_engine_apply_plock_delta(...)` si exposé.
- **LEDs SEQ** : lecture **uniquement** via `seq_led_bridge`; plus aucun accès clock dans le renderer SEQ.

---

## 📥 Procédure d’acquisition des sources (obligatoire)

> **Accès libre à l’ensemble du dépôt.** Tu peux ouvrir et analyser tous les fichiers nécessaires sans limitation de nombre, tant que tu respectes les autres invariants et documentations existants.

---

## 🧪 Plan de tests minimal

- **Unitaires (host)** : model helpers (set/get/toggle), p-locks, micro-timing, offsets.
- **Intégration (target ou sim)** :  
  - progression playhead vs. clock 24 PPQN ;  
  - polyrythmie (longueurs voix distinctes) ;  
  - REC live : binding V1..V4 + NOTE_OFF garanti ;  
  - P-locks CART : `t_cc = max(now, t_on - tick_st/2)` ;  
  - snapshot immuable → rendu UI stable (aucun deadlock/lock inversé).

---

## ✅ Definition of Done (DoD)

- [ ] Fichiers complets **C/C++ + headers** avec **guards** et **Doxygen**.  
- [ ] Respect strict des **invariants Brick** (UI sans I/O ; bus confiné ; renderer pur).  
- [ ] Build OK (ChibiOS 21.11.x, STM32F429).  
- [ ] **NOTE_OFF jamais droppé** ; poly/retrigger safe.  
- [ ] `seq_runtime` publié, lu sans lock par l’UI ; `seq_led_bridge` thread-safe.  
- [ ] Intégration UI : preview hold, deltas p-locks, LEDs via bridge uniquement.  
- [ ] Tests min. passés + guide d’intégration mis à jour.
