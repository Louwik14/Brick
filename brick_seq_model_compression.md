# Étape 3 — Compression du modèle SEQ

## Résumé des transformations
- Compactage des structures du modèle séquenceur : types `enum` réduits en entiers sur 8 bits, voix ramenées à 5 octets et p-locks à 8 octets pour chaque step, ce qui diminue la taille cumulée du motif en mémoire.【F:core/seq/seq_model.h†L34-L120】
- Séparation de l’état LED du motif : l’état `seq_led_bridge_state_t` référence désormais un motif partagé en CCM, expose un index de piste et centralise les incréments de génération via des helpers dédiés.【F:apps/seq_led_bridge.c†L49-L78】
- Ajustements du pont LED pour travailler sur le motif référencé (accès, commit des slots hold, publication runtime) sans dupliquer les steps, tout en conservant les API publiques (`seq_led_bridge_access_pattern`, `seq_led_bridge_get_pattern`, `seq_led_bridge_get_generation`).【F:apps/seq_led_bridge.c†L132-L204】【F:apps/seq_led_bridge.c†L703-L900】【F:apps/seq_led_bridge.c†L1181-L1243】
- Initialisation du mode clavier synchronisée avec l’UI LED afin de préserver la compatibilité des tests host et de préparer les futures intégrations multi-pistes.【F:apps/ui_keyboard_app.c†L218-L233】

## Tailles `arm-none-eabi-size`

| Build | text | data | bss | dec |
| --- | --- | --- | --- | --- |
| Avant (après Étape 2) | 159764 | 32776 | 163832 | 356372 |
| Après Étape 3 | 159424 | 33304 | 163296 | 356024 |

> *Mesures « Avant » reprises du rapport Étape 2, « Après » issues du build `make -j4` (toolchain GCC 13.2.1).*【F:brick_memory_audit.md†L93-L104】【09b8ef†L1-L9】

## Empreinte des structures clés

| Structure | Avant | Après |
| --- | --- | --- |
| `seq_model_step_t` | 428 octets | 222 octets |
| `seq_model_plock_t` | 16 octets | 8 octets |
| `seq_model_voice_t` | 8 octets | 5 octets |
| `seq_model_pattern_t` | 27 424 octets | 14 224 octets |

Mesures obtenues par compilation ciblée des en-têtes historiques et actuels pour estimer l’impact brut de la refactorisation.【f19117†L1-L4】【6c041d†L1-L4】

## Déploiement mémoire
- Le motif séquenceur et ses buffers hold restent cantonnés à la CCM via `g_pattern` et les structures CCM existantes, libérant la SRAM principale pour les buffers DMA.【F:apps/seq_led_bridge.c†L49-L104】
- Les slots hold et caches cart sont toujours exposés en CCM (`CCM_DATA`), mais s’appuient désormais sur le motif partagé au lieu de copies indépendantes, ce qui réduit la duplication lors des prévisualisations hold.【F:apps/seq_led_bridge.c†L49-L104】

## Tests
- `make check-host`
- `make -j4`
