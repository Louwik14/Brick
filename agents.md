## 🔑 Priorité & portée
- **Les prompts récents sont la source de vérité absolue.** Ce fichier ne fait que
  donner un contexte minimal et **ne doit jamais contredire** une instruction du prompt.
- **Respecter strictement la portée demandée par le prompt.** S’il demande “Étape X”,
  **n’implémente que l’Étape X** (pas d’étapes suivantes, pas d’initiatives hors périmètre).

## 🧭 Cible matérielle (détection)
- La cible MCU (STM32F429, STM32F767, etc.) **doit être déduite du dépôt**:
  - scripts/Makefile, linker, headers `STM32F4xx`/`STM32F7xx`,
  - répertoire `board/` du projet.
- **Ne fige pas** la cible dans ce fichier. Si le repo indique F7, on traite F7; sinon F4.
- Le board utilisé est **celui du projet** (ex. `./board/board.mk`), pas un board générique
  de ChibiOS, sauf instruction contraire.

## ⚙️ Architecture fonctionnelle (rappel non prescriptif)
- Organisation: Model / Engine / UI / Cart / MIDI, ChibiOS 21.11.x.
- Sequencer event-driven (tick tempo → engine + LED bridge).
- **Aucune opération lourde en ISR**; moteurs coopératifs (UI/transport/recorder).

## 🎼 Règles musicales invariables
- **Ne jamais envoyer “All Notes Off”** hors cas explicitement demandés (ex. STOP/panic).
- Les p-locks **SEQ** (note/vel/len/microtiming) sont “musicaux” et ne transforment pas
  un step en automation; les p-locks **CART** définissent l’automation.
- L’UI reflète le modèle (classification & couleurs), ne force pas d’état.

## 💾 Mémoire (guides, sans imposer d’actions)
- Respecter les instructions du prompt pour la RAM (ex. Étape 2 “CCM/const/DMA”).
- **DMA vs RAM** :
  - F4: **CCMRAM** (64 KB) non DMA; SRAM principale DMA-safe.
  - F7: **DTCM** non DMA; **AXI SRAM** DMA-safe; SRAM1/2 DMA-safe.
- Si le prompt demande migration mémoire: **ne déplacer en CCM/DTCM que ce qui n’est pas
  lu/écrit par DMA/USB**; garder les buffers DMA en SRAM DMA-safe.
- En cas de doute, **ne pas migrer** et documenter (le prompt prime).

## 🧪 Discipline d’exécution
- Si le prompt demande des **patchs concrets**: produire des diffs/patchs, pas du conseil.
- Si le prompt exige un **rapport ou mesures**: compiler et inclure `size`, extraits `.map`,
  AVANT/APRÈS, uniquement dans le cadre de l’étape demandée.
- **Ne change pas d’API publique** sauf instruction explicite.

## ✅ Résumé d’intention
- Suivre **strictement** le dernier prompt (étape, fichiers, limites).
- Utiliser **le board du projet** et la cible MCU déduite du repo.
- Appliquer les **caveats DMA** lors des migrations mémoire.
- Conserver la **stabilité musicale** (pas d’arrêt ni reset implicite).
