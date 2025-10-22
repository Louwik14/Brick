# Rapport — QuickStep : NOTE_ON mangé sur même note consécutive

## 1. Résumé d'architecture

Le séquenceur Brick repose sur un pipeline **Reader → Runner** : le `reader` expose des vues immuables de la pattern active, et le `runner` (apps) itère les 16 tracks à chaque tick, déclenche NOTE_OFF/NOTE_ON et applique les p-locks cartouche, le tout sans accès direct au runtime cold.【F:docs/ARCHITECTURE_FR.md†L101-L112】 La spec garantit 4 voix par step, NOTE_OFF systématiques et un quick-step neutre qui doit jouer la voix 1 (C4, vel 100, len 1).【F:SEQ_BEHAVIOR.md†L8-L40】

## 2. Chronologie Reader → Runner d'un step

### 2.1 Pipeline du tick

`seq_engine_runner_on_clock_step()` récupère le bank/pattern actif via `seq_led_bridge`, calcule `step_abs`/`step_idx`, puis parcourt les 16 tracks :

```c
for track in 0..15:
    handle = seq_reader_make_handle(bank, pattern, track)
    _runner_handle_step(track, step_abs, step_idx, handle)
    _runner_apply_plocks(handle, step_idx, cart)
```

【F:apps/seq_engine_runner.c†L116-L136】

### 2.2 Pseudo-code du cœur `_runner_handle_step`

1. **Boundary OFF par slot** : si `state->active` et `step_abs >= off_step`, on envoie NOTE_OFF et on libère l'état pour ce *slot* uniquement.【F:apps/seq_engine_runner.c†L195-L207】
2. **Mute track** : un mute force un flush complet puis stop.【F:apps/seq_engine_runner.c†L209-L219】
3. **Lecture Reader** : `seq_reader_get_step()`/`_runner_step_has_voice()` valident que le step contient des voix reproductibles, sinon on sort.【F:apps/seq_engine_runner.c†L222-L229】【F:core/seq/reader/seq_reader.c†L79-L158】
4. **Boucle voix** : pour chaque slot Reader actif (`voice_view.enabled`) on :
   - convertit note/vel/length,
   - si l'état local du même slot est encore actif → NOTE_OFF (slot),
   - NOTE_ON (slot), stockage de `note` et `off_step = step_abs + length`.

【F:apps/seq_engine_runner.c†L231-L264】

Ainsi, l'automate ne consulte que les vues Reader et les caches hot (`s_note_state`) et garantit l'ordre OFF (slot) → ON (slot) pour chaque voix.

## 3. Analyse des pré-conditions de NOTE_ON

* **Activation de la voix** : `seq_reader_get_step_voice()` ne positionne `enabled=true` que si la voix est `SEQ_MODEL_VOICE_ENABLED` et `velocity>0`; les flags de step (`HAS_VOICE`, `AUTOMATION_ONLY`) sont recalculés par `seq_model_step_set_voice()` lors des écritures QuickStep/recorder, ce qui assure l'entrée dans la boucle voix dès qu'une note est présente.【F:core/seq/reader/seq_reader.c†L79-L158】【F:core/seq/seq_model.c†L101-L150】
* **Écriture QuickStep/Clavier** : `seq_recorder` réserve un slot logique par note active, relaie les NOTE_ON/OFF vers `seq_live_capture`, qui instancie ou met à jour le step cible, fixe la voix (note/vel/len) et marque le slot comme actif pour la durée mesurée.【F:apps/seq_recorder.c†L31-L140】【F:core/seq/seq_live_capture.c†L200-L303】
* **Allocation de slot** : `_seq_live_capture_pick_voice_slot()` réutilise un slot si la même note existe déjà *dans le même step*, sinon il prend le premier slot libre (ou round-robin si plein). Il n'existe aucune contrainte pour conserver le même slot qu'au step précédent : deux steps consécutifs peuvent porter la même note sur **des slots différents** selon les autres voix déjà occupées.【F:core/seq/seq_live_capture.c†L439-L467】

## 4. Gestion “même note” et identification du défaut

### 4.1 Scénario observé

1. **Step N** : slot 1 joue C4 avec une longueur >1 (cas fréquent en capture : la durée réelle est convertie en `length_steps` via `_seq_live_capture_compute_length_steps`). L'état runner garde `state[track][1] = {active=true, note=C4, off_step=N+len}`.【F:core/seq/seq_live_capture.c†L218-L243】【F:apps/seq_engine_runner.c†L240-L264】
2. **Step N+1** : la même hauteur C4 est écrite sur un slot différent (ex. slot 0) parce que le step contient déjà une autre voix sur slot 1 ou que le recorder a choisi un slot libre différent. Avant NOTE_ON, `_runner_handle_step()` n'a libéré que les slots dont `off_step <= step_abs`. Si `len>1`, le slot 1 reste actif (pas d’OFF boundary). Lors de la boucle voix, le runner :
   - ne voit pas l'activité de C4 sur le slot 1 (car il ne scrute pas les autres slots),
   - envoie NOTE_ON C4 sur le slot 0.

3. **Conséquence MIDI** : le périphérique reçoit un NOTE_ON C4 alors qu'une note identique est déjà active sur le même canal (slot 1). De nombreux synthés considèrent alors l'événement comme redondant : pas de retrigger d'enveloppe, la note semble “mangée”.

### 4.2 Preuve dans le code

* L'état MIDI `s_note_state` est indexé **par slot** (pas par hauteur) ; aucune recherche « par note » n'est effectuée avant NOTE_ON.【F:apps/seq_engine_runner.c†L48-L68】【F:apps/seq_engine_runner.c†L195-L264】
* `_runner_handle_step()` n'émet des NOTE_OFF anticipés que pour le même slot ou quand la track est mutée ; il n'existe pas de purge croisée des notes encore actives sur d'autres slots partageant la même hauteur.【F:apps/seq_engine_runner.c†L195-L264】
* QuickStep/Live Rec peuvent légitimement produire la configuration problématique : longueur >1 (note tenue) + nouvelle occurrence sur un slot différent via `_seq_live_capture_pick_voice_slot()` qui n'applique qu'une politique locale au step.【F:core/seq/seq_live_capture.c†L200-L303】【F:core/seq/seq_live_capture.c†L439-L467】

### 4.3 Hypothèses écartées

* **Flags HAS_VOICE** incorrects : invalidé car `seq_model_step_set_voice()` force `seq_model_step_recompute_flags()`. Les steps incriminés portent bien `HAS_VOICE` et passent le garde runner.【F:core/seq/seq_model.c†L101-L150】
* **Mute ou bank/pattern** : `seq_led_bridge_get_active()` est partagé par QuickStep et Runner, aucune permutation n'est observée uniquement pour “même note consécutive”.
* **Velocity 0** : les vues Reader filtrent `vel=0` et la synthèse note sur slot 0/1 reste `enabled=true` dans les cas reproduits.【F:core/seq/reader/seq_reader.c†L131-L158】

## 5. Diagnostic tranché

Le NOTE_ON du step N+1 est ignoré parce que le runner ne désactive que la note tenue **sur le même slot** avant de rejouer, laissant actifs les slots voisins qui peuvent porter la même hauteur suite aux allocations QuickStep/Live Rec. En présence d'une note précédente (même pitch) encore active sur un autre slot, aucun NOTE_OFF n'est émis, le périphérique MIDI garde la note “ouverte” et ignore la nouvelle impulsion — d'où la sensation de note mangée. Tout le chemin Reader/flags est correct ; le défaut réside dans la politique de purge des notes dans `_runner_handle_step()` qui n'est pas *pitch-safe*.

## 6. Correctif minimal runner-only recommandé

Ajouter une étape de **purge par hauteur** juste avant le NOTE_ON d'un slot :

1. Scanner les autres slots du track (`s_note_state[track][other]`) ;
2. Si un slot actif transporte la **même note** (`state->note == note`) et que ce n'est pas le slot courant, émettre immédiatement NOTE_OFF et libérer cet état ;
3. Ensuite appliquer la logique existante (NOTE_OFF du slot courant si besoin → NOTE_ON → mise à jour de `off_step`).

Pseudo-patch illustratif :

```c
for each slot:
    ... lire voice_view ...
    uint8_t note = ...;
    for (uint8_t other = 0; other < VOICES; ++other) {
        if ((other != slot) && s_note_state[track][other].active &&
            (s_note_state[track][other].note == note)) {
            _runner_send_note_off(track, note);
            s_note_state[track][other] = {0};
        }
    }
    // logique actuelle (OFF slot courant puis ON)
```

Cette correction respecte les garde-fous : elle reste confinée au runner (hot), ne touche pas le runtime cold, n'introduit pas d'accès supplémentaires, et garantit qu'un NOTE_ON rejouant une hauteur retrouve toujours un état MIDI neutre.

## 7. Plan de tests host

1. **Nominal** — Step N = C4 len=1, Step N+1 = C4 (même slot) : vérifier via `midi_probe` que l'ordre est `OFF(C4)` à `step N+1` puis `ON(C4)` (track non mutée).【F:apps/midi_probe.c†L1-L58】【F:apps/seq_engine_runner.c†L195-L264】
2. **Cross-slot** — Step N : C4 len=2 sur slot 1, Step N+1 : C4 sur slot 0. Attendu : `OFF(C4)` (issu de la purge par note) puis `ON(C4)` au même tick, `s_note_state` nettoyé sur les deux slots.
3. **Stress** — Pattern 512 steps générée en host (script) avec probabilité 0,3 de répétition de note et rotation de slots (simulateur en injectant `seq_live_capture`). Attendu : aucune note manquante, `midi_probe` montre toujours OFF→ON avant chaque retrigger homonyme ; pas de NOTE_OFF orphelin.

---

**Conclusion** : Le comportement erratique “NOTE_ON mangé” ne provient pas du Reader mais du runner qui n'assure pas la neutralisation croisée des notes par hauteur. Un balayage par pitch avant NOTE_ON, limité au runner, suffit à restaurer les invariants MIDI sans toucher au modèle ni au runtime cold.
