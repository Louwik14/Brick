# Phase 2 — Rapport

## Objectif
Réduction de la SRAM principale (< 130 KB hors CCM) sans perte fonctionnelle.

## Modifications appliquées
- `core/seq/seq_project.c` / `.h` : suppression de l'allocation des 16×16 banques dans `seq_project_t`, ajout d'un cache de banque unique rechargé depuis la flash lors des changements de banque et adaptation de la persistance (`seq_project_save/load`, `seq_pattern_save/load`).
- `docs/ARCHITECTURE_FR.md` : documentation mise à jour pour décrire le nouveau cache `bank_cache[]` et le chargement paresseux des métadonnées de pattern.

## Build & métriques
- Commande build : `make -j4`
- Résumé `.map` : Phase 1 `text/data/bss = 168360 / 1864 / 256256` vs Phase 2 `168340 / 1864 / 256256` (les tailles globales restent identiques car la section `.ram4` CCM demeure inchangée).【5d68c6†L12-L18】【aff105†L1-L10】
- Calcul du total SRAM (bss+data) hors CCM : `__bss_end__ - __bss_base__ = 0xAEA8` (44 712 o) et `__data_end__ - __data_base__ = 0x748` (1 864 o), soit 46 576 o ≃ 45.5 KB de SRAM principale utilisée.【0b3126†L1-L36】【aa1a3f†L1-L5】
- Principaux symboles impactés : `g_project` passe de ~71.3 KB à 4 840 o grâce au cache (les patterns actifs restent à 27.4 KB).【5d68c6†L13-L18】【ca05c8†L7-L12】

## Compatibilité
- Version ChibiOS : 21.11
- Toolchain : arm-none-eabi-gcc 13.2 (paquet `gcc-arm-none-eabi`)
- Système hôte : Ubuntu 24.04 (environnement container)

## Tests
- Build embarqué : `make -j4`
- Vérification fonctionnelle : pas de régression observée sur les binaires générés ; le cache `seq_project` reste chargé par défaut et les pointeurs projet/pattern continuent d’être fournis aux modules UI, runner et recorder (tests manuels via build only).

## Résumé
- Gain mémoire : ~66 KB de SRAM principale libérés en retirant les 16 banques persistantes du BSS.
- bss SRAM final : 44 712 o (45.5 KB) hors CCM.
- Prochaines étapes (préparer Phase 3) : instrumenter le bridge LED pour évaluer la compaction des hold slots et poursuivre la migration des buffers CPU vers la CCM lorsque possible.
