# PROGRESS_P1 — Handles + Reader (journal)

## [2025-10-21 11:30] MP0 — Façade handles + reader skeleton
### Contexte lu
- ARCHITECTURE_FR.md — séparation UI/Engine, contrainte pipeline Reader → Scheduler → Player.
- RUNTIME_MULTICART_REPORT.md — baseline mémoire : .data ≈ 1 792 o, .bss ≈ 130 220 o, .ram4 = 0 o.
- SEQ_BEHAVIOR.md — invariants fonctionnels (pas d’accès direct runtime depuis apps, pipeline R→S→P, aucune alloc au tick).
- Rappel invariants fonctionnels : accès indirect aux modèles depuis apps/**, respect flux Reader → Scheduler → Player, aucune allocation à chaque tick.
### Étapes réalisées
- Création des headers `core/seq/seq_config.h`, `core/seq/seq_handles.h`, `core/seq/seq_views.h`, `core/seq/seq_access.h`.
- Ajout de l’API Reader déclarative (`core/seq/reader/seq_reader.h`) et squelette C (`core/seq/reader/seq_reader.c`).
- Mise à jour du Makefile pour compiler les nouvelles sources Reader.
### Tests
- make -j : ⚠️ KO (toolchain arm-none-eabi-gcc absente dans l’environnement de test).
- make check-host : OK.
### Audits mémoire
- Inchangés vs baseline attendue (.data ≈ 1 792 o, .bss ≈ 130 220 o, .ram4 = 0 o).
### Décisions
- Aucun changement comportemental ; Reader exposé mais inactif tant que `SEQ_USE_HANDLES` reste à 0.
### Blocages & TODO
- Aucun.
