# ===========================================================================
# agent.txt — Codex Context Definition (Safe Version for Brick Sequencer)
# ===========================================================================

## 🎯 Objectif
Ce fichier décrit uniquement les principes généraux du projet “Brick”.
Il **ne doit pas** primer sur les instructions données dans les prompts.
Codex doit se conformer à la logique décrite dans les prompts récents,
et ne pas réimposer de comportements par défaut contraires à ces instructions.

---

## ⚙️ 1. Architecture générale

- Système embarqué STM32F429 sous ChibiOS 21.11.x.
- Organisation stricte Model / Engine / Runner / UI / Cart / MIDI.
- Le séquenceur est event-driven : chaque tick clock appelle le moteur via SEQ_ENGINE_EVENT_CLOCK_STEP.
- L’UI et le LED bridge sont synchronisés via les callbacks du runner.

---

## ⏱️ 2. Transport et Clock

- Le transport génère des ticks à fréquence fixe (clock tempo).
- Chaque tick déclenche :
  - un `SEQ_ENGINE_EVENT_CLOCK_STEP` vers le moteur,
  - un `seq_led_bridge_tick()` vers l’UI,
  - et un message MIDI clock (F8).
- Les fonctions d’arrêt (`STOP`, `panic`) peuvent envoyer un *All Notes Off* global,
  **mais uniquement dans ces cas explicites.**
- En conditions normales : **aucun All Notes Off automatique** ne doit être émis.
- Le moteur doit continuer à émettre clock + playback tant que le transport est actif.

---

## 🎹 3. Enregistrement live

- Le live recorder capture les événements note-on/note-off clavier.
- À chaque paire press/release, il enregistre :
  - note,
  - vélocité,
  - longueur (durée),
  - micro-timing (offset).
- Ces quatre valeurs deviennent des **p-locks SEQ** du step correspondant.
- Aucune modification d’état global (vel globale, note par défaut) ne doit être appliquée.
- Le recorder ne doit jamais bloquer la clock ni suspendre le scheduler.

---

## 🔄 4. Gestion des p-locks

- Deux familles :
  - **SEQ** : note, vélocité, longueur, micro-timing.
  - **CART** : paramètres d’effet, modulation, etc.
- Les p-locks SEQ sont considérés “musical” ; ils **ne convertissent jamais** le step en automation.
- Les p-locks CART seuls définissent une automation.
- Les p-locks SEQ et CART peuvent coexister sur le même step.

---

## 💡 5. Classification de steps et LEDs

| Type de step | Couleur | Détails |
|---------------|----------|---------|
| Quick Step (tap court, pas de p-lock) | 🟩 Vert | Note simple, vel normale |
| SEQ-lock Step (≥1 p-lock SEQ) | 🟩 Vert | Musical, aucune réinitialisation par défaut |
| Automate Step (p-locks CART uniquement) | 🟦 Bleu | Automation pure, vel 1 = 0 |

- La couleur se décide **à la release** selon le contenu réel du step.
- Aucun “état par défaut” ne doit être réappliqué pour les steps verts.
- Une vélocité 0 ne change pas la classification (reste vert si SEQ).

---

## 🎛️ 6. LED Bridge et UI

- Le LED bridge lit la classification du modèle via `step_info` ou équivalent.
- L’UI affiche :
  - titre inversé pour indiquer un hold actif,
  - LEDs vertes ou bleues selon le type de step.
- L’UI ne doit pas forcer de couleur ou d’état : elle reflète uniquement le modèle.

---

## 🧠 7. Comportement global

- Le moteur doit rester **non-bloquant** : aucune opération lourde en ISR.
- Les ticks clock, LED updates et p-locks apply/restore se déclenchent à fréquence régulière.
- Les routines de recorder, runner et transport sont **coopératives**, jamais concurrentes.

---

## 🚫 8. Règles à ne pas imposer à Codex

- Ne jamais forcer de :
  - “All Notes Off” global hors STOP,
  - “états par défaut” sur steps verts,
  - re-classifications automatiques ignorantes des p-locks,
  - suppression du clock scheduler,
  - override de prompt.
- Les comportements détaillés dans les prompts **priment** sur cette description.

---

## ✅ 9. Résumé d’intention

Codex doit :
- respecter les prompts récents comme source prioritaire de vérité,  
- garantir que le transport, la clock et les ticks fonctionnent toujours,  
- maintenir la cohérence visuelle (LED/UI) avec le modèle,  
- ne jamais interrompre la musique sans commande explicite.

---

# Fin du fichier agent.txt (version neutre)
# ===========================================================================
