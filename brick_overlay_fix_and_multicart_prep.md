# Étape 4 — Correctifs overlays & préparation multi-cart

## Résumé
- Correction de l’écrasement du `cart_name` dans les overlays SEQ/ARP en dissociant la cartouche hôte et la spécification d’overlay.
- Introduction d’un **contexte d’overlay** unifié (`ui_overlay_session_t`) qui centralise cartouche hôte, état restauré, overrides et tag.
- Ajout d’un accès explicite à la cartouche hôte (`ui_overlay_get_host_cart`) et mise à jour du backend pour l’utiliser lors de la préparation bannière.
- Préparation de la future structure `seq_project_t`/multi-cart : la cartouche hôte reste traçable pendant toute la durée de vie d’un overlay et les overrides peuvent être réappliqués sans duplication.

## Causes du bug `cart_name` / `overlay_tag`
- `ui_overlay_prepare_banner()` choisissait le texte de bannière à partir de `ui_get_cart()`.
- Lorsque l’overlay SEQ/ARP était déjà actif, `ui_get_cart()` renvoyait la **spec de l’overlay** (`seq_ui_spec`, `arp_ui_spec`) et non la cartouche custom d’origine.
- `ui_overlay_set_banner_override()` recevait donc des noms comme `"SEQ UI"` ou `"ARP UI"`, ce qui écrasait `cart_name` dans l’UI.

## Correctifs apportés
1. **Contexte d’overlay structuré**
   - Nouveau `ui_overlay_session_t` (état interne unique) :
     - `host_cart` et `host_state` pour restaurer fidèlement la cartouche d’origine.
     - `banner_cart_override` / `banner_tag_override` pour conserver les libellés corrects.
     - `custom_mode` et `id` stockés dans la même structure pour éviter les incohérences.
2. **Accès à la cartouche hôte**
   - Nouvelle API `ui_overlay_get_host_cart()` exposée depuis `ui_overlay.h`.
   - `ui_backend` l’utilise pour préparer la bannière SEQ/ARP/KBD et éviter toute fuite de spec overlay.
3. **Préparation bannière robuste**
   - `ui_overlay_prepare_banner()` détecte désormais les cas où `prev_cart` pointe vers la spec d’overlay et bascule automatiquement vers la cartouche hôte conservée.
   - Fallback systématique sur le nom de la cartouche hôte (ou `"UI"` par défaut) lorsqu’aucun nom valide n’est fourni.

## Préparation multi-cart / `seq_project_t`
- Le contexte d’overlay encapsulé rend explicite la relation **overlay ↔ cartouche hôte** ; il pourra être étendu vers un tableau de cartouches (`seq_project_t`) sans devoir refactorer les appels existants.
- Les overrides sont maintenus même lors d’un `ui_overlay_enter()` consécutif, ce qui simplifie les futurs cycles multi-cart.
- `ui_backend` consulte désormais la cartouche hôte via une API claire, évitant de propager des pointeurs internes et facilitant l’insertion d’un routeur multi-cart.

## Tests
- `make check-host`
- `make -j4`
