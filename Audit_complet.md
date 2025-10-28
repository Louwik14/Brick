1) Vue d’ensemble

Brick pilote une interface matérielle (boutons, encodeurs, OLED) et route tous les paramètres vers des cartouches DSP externes via un backend neutre ; l’appareil n’émet aucun son et prépare l’arrivée d’un séquenceur polyphonique avec p-locks et micro-timing.

Le séquenceur attendu gère 16 tracks de 64 steps, 4 voix par step, des p-locks internes/cartouches, un pipeline Reader → Scheduler → Player sans NOTE_OFF perdu, et une UI stateless à base de snapshots.

Cartographie des couches principales
Couche	Rôle	Dépendances clés	Références
UI (ui/backend, ui/controller, ui_renderer)	Collecte les entrées, gère les modes, déclenche les holds/p-locks	ui_backend → seq_led_bridge, cart_link, MIDI	
Apps (apps/seq_led_bridge, apps/seq_engine_runner)	Pont Reader-only vers LEDs/hold et Runner temps réel	seq_led_bridge → seq_model, seq_plock_pool; seq_engine_runner → seq_reader, cart_link	
Core/Seq (core/seq/*)	Modèle des tracks/steps, runtime partagé, reader	seq_model, seq_project, seq_reader, seq_runtime	
Core/Cart (core/cart_link.c)	Shadow des paramètres cartouche, envoi UART	Attend des IDs 16 bits (≤512)	
Spécifications (core/spec/cart_spec_types.h)	Définit des paramètres cart 16 bits utilisés par UI et cart link		
Runtime (core/seq/seq_runtime.c, seq_runtime_cold.c)	Héberge seq_project + 16 tracks, expose vues cold		
2) Dépendances & Couplage
Principales inclusions (hot path)
Fichier	Inclut	Observations
ui/ui_backend.c	seq_led_bridge.h, cart_link.h, midi.h	UI couche haute dépend directement du pont hold et des services cart/MIDI.
apps/seq_led_bridge.c	core/seq/seq_model.h, seq_plock_pool.h, seq_project_access.h	Pont LED touche le modèle interne et le pool ; c’est la porte d’entrée UI→SEQ.
apps/seq_engine_runner.c	core/seq/seq_access.h, seq_plock_ids.h, cart_link.h	Runner mélange Reader, cart link et MIDI probe ; dépendances multiples.
core/seq/seq_reader.c	seq_project.h, seq_runtime.h, seq_plock_pool.h	Reader dépend encore du runtime legacy et du pool.
core/seq/seq_project.c	seq_reader.h, seq_plock_pool.h	Module “cold” (persistence) inclut le reader hot → couplage inverse.
core/cart_link.c	seq_runtime.h, seq_project.h (via _cart_link_assign_tracks)	Cart link remonte jusqu’au runtime pour assigner les tracks.
Graphes Mermaid
Inclusion / ownership majeur

graph LR
  UI["ui_backend.c"] --> Bridge["apps/seq_led_bridge.c"]
  UI --> Runner["apps/seq_engine_runner.c"]
  Bridge --> Model["core/seq/seq_model.c"]
  Bridge --> Pool["core/seq/seq_plock_pool.c"]
  Runner --> Reader["core/seq/reader/seq_reader.c"]
  Reader --> Runtime["core/seq/seq_runtime.c"]
  Runtime --> Project["core/seq/seq_project.c"]
  Runner --> Cart["core/cart_link.c"]
  Cart --> Spec["core/spec/cart_spec_types.h"]

    UI appelle le pont hold pour écrire dans le modèle/pool.

Runner lit via seq_reader et publie vers cart link.

Cart link attend les IDs 16 bits décrits dans les specs cart.
Appels haut niveau

graph LR
  Hold["UI Hold"] -->|param_changed| BridgeApply["seq_led_bridge_apply_*"]
  BridgeApply -->|seq_model_step_set_plocks_pooled| ModelOps
  RunnerTick["Clock tick"] -->|seq_reader_*| ReaderOps
  ReaderOps -->|seq_plock_pool_get| Pool
  RunnerTick -->|cart_link_param_changed| CartIO

    ui_backend_param_changed délègue aux apply du bridge pendant un hold.

seq_led_bridge_apply_* collecte/commit via seq_model_step_set_plocks_pooled (pool).

Le Runner (tick) lit les mêmes steps/pool et pousse vers cart link.
Cycles et violations de couches

    core/seq/seq_project.c ↔ core/seq/reader/seq_reader.c : le project (persistant) inclut le reader pour filtrer les p-locks PLK2, tandis que le reader réutilise seq_project_get_track_const via seq_runtime_blocks. Couplage bidirectionnel froid ↔ hot.

cart_link.c dépend du runtime seq pour assigner les tracks, ce qui expose le seq_project interne hors de core/seq. Accepté aujourd’hui mais brise la séparation annoncée dans README.
Instabilité relative (modules les plus centraux → plus à risque)

    core/seq/seq_model.c: Touché par UI, reader, live capture, codec (forte centralité).

apps/seq_led_bridge.c: Au croisement UI ↔ modèle/pool ; encode/décode p-locks pour toutes les écritures.

apps/seq_engine_runner.c: Seul producteur d’événements MIDI/cart à l’exécution ; dépend de nombreux modules.

core/seq/seq_project.c: Gère serialization + policy cart, manipule pool et runtime.

core/cart_link.c: Interface unique vers les cartouches ; attend IDs 16 bits.
3) Séquenceur (model / reader / runner)

    Structures clés – seq_model_step_t contient 4 voix, un pl_ref {offset,count} packé sur 3 octets, des offsets globaux et des flags automation/playable.


Les tracks (16 max) résident dans g_seq_runtime, aux côtés du seq_project.

Helpers modèle – seq_model_step_set_plocks_pooled recopie un tableau plk2_t vers le pool, réutilise l’offset seulement si le nombre d’entrées ne change pas (sinon nouvelle allocation). Aucun contrôle explicite sur la capacité restante.

Les compteurs d’activité/automation sont recalculés après mutation.

Reader – _resolve_legacy_track récupère la track active depuis les alias seq_runtime_blocks; seq_reader_plock_iter_next lit les entrées du pool à partir du couple (offset,count) et restitue {id,value,flags} en 8 bits, avec un état global s_plock_iter_state (non réentrant).

Runner – _runner_step_ctx_process_plocks parcourt les entrées via seq_reader_pl_*, convertit les IDs et appelle _runner_apply_cart_plock/_runner_apply_midi_plock. Les cart p-locks sont routés via cart_link_param_changed après conversion d’ID.

Namespace des paramètres

    seq_plock_entry_t (alias plk2_t) stocke param_id et value en uint8_t, flags en uint8_t. La distinction interne/cart repose sur PL_INT_* (0x00–0x3F) vs encodage cart (0x40+).

Les specs cart et cart_link manipulent pourtant des uint16_t id (jusqu’à 512).

Double source de vérité / legacy

    Les macros #pragma GCC poison plocks/plock_count sont présentes dans seq_model, seq_reader, seq_led_bridge, seq_live_capture pour empêcher l’usage des tableaux legacy ; les branches #if !SEQ_FEATURE_PLOCK_POOL subsistent mais sont inactives par défaut.

    seq_project.c encode/décode PLK2 en 3 octets (id,value,flags), sans autre format.

Hot/cold

    g_seq_runtime contient à la fois le seq_project (froid) et les tracks (chaud) ; les alias hot/cold sont mis à jour à l’initialisation via seq_runtime_layout_*. Aucune séparation mémoire encore effectuée (alias identiques).

seq_runtime_cold_view protège l’accès en phase TICK (assert host) mais se repose sur seq_rt_phase pour la discipline.
4) P-locks & Pool
Chemin d’écriture (UI → pool)

    ui_backend_param_changed détecte un hold (mask ≠0) et appelle seq_led_bridge_apply_plock_param (params SEQ) ou seq_led_bridge_apply_cart_param (params cart) avec les valeurs UI.

Chaque *_apply_* :

    Collecte le contenu courant via _seq_led_bridge_collect_plocks et prépare un buffer (entries[24]).

Ajuste le step (mode neutral/automation, voix primaires) et met à jour offsets/voix.

Encode les p-locks : internes → _seq_led_bridge_buffer_upsert_internal, cart → _seq_led_bridge_buffer_upsert_cart. Les cart IDs sont réduits à 0x40 + (parameter_id & 0xFF) et saturés à 0xFF.

Commit via seq_model_step_set_plocks_pooled, ce qui écrit dans seq_plock_pool et met à jour pl_ref sur le step.

La génération de track est incrémentée, seq_led_bridge_publish() alimente le rendu UI/LED.
Chemin de lecture (runner)

    seq_engine_runner_on_clock_step construit un contexte par track et lit le step courant via seq_reader_get_step, seq_reader_pl_next (8 bits).

Les cart p-locks passent par _runner_apply_cart_plock qui traite l’ID 8 bits :

    Si flags indique SEQ_READER_PL_FLAG_DOMAIN_CART, il envoie param_id = id (donc la valeur 0x40–0xFF stockée dans le pool).

    Sinon param_id = pl_cart_id(id) (soustraction 0x40).

cart_link_param_changed attend des IDs 16 bits <512 et maintient un shadow [CART_COUNT][512]. Si on lui envoie 0x40–0xFF, la cartouche reçoit un index décalé. Les paramètres >255 sont tronqués (au mieux) ou mappés sur un ID invalide.
Hypothèse “p-locks cart cassés”

    Encodage 8 bits : seq_led_bridge_buffer_upsert_cart tronque le parameter_id à 8 bits puis ajoute 0x40.

seq_engine_runner renvoie ce même octet (car flags indique domaine cart) à cart_link_param_changed, qui croit recevoir un ID complet.

    Les paramètres cart déclarés en 16 bits ne peuvent donc pas être p-lockés correctement (IDs ≥256 ou simplement ≠ [0x40,0xFF] attendus côté protocole). C’est cohérent avec les symptômes “p-locks cart ne marchent plus”.

Gestion pool

    seq_plock_pool_alloc est monotone : s_used est seulement incrémenté ; aucune libération ni reset hors tests.

seq_model_step_set_plocks_pooled ne réutilise l’offset que si la nouvelle taille égale l’ancienne ; toute variation alloue un segment inédit sans libérer l’ancien.

Aucun reset n’est invoqué lors d’un clear pattern/track dans le firmware (uniquement dans les tests). Le pool peut donc s’épuiser après de nombreuses éditions (erreur -1 → rollback + warnings).
Checklist d’assertions recommandées (sans code)

    Vérifier pl_ref.count <= SEQ_MAX_PLOCKS_PER_STEP à chaque commit et sur lecture reader.

    Vérifier pl_ref.offset + pl_ref.count <= seq_plock_pool_used() avant d’accéder au pool.

    Asserter parameter_id < CART_LINK_MAX_DEST_ID avant d’encoder un p-lock cart et stocker l’ID complet (=> redéfinir plk2_t).

    Déclencher un reset pool (seq_plock_pool_reset) lors d’un seq_model_track_init/seq_project_clear_track.

    Interdire s_used proche de la capacité (warning) pour prévenir la saturation.

5) Mémoire & performances

    Réservoir principal : g_seq_runtime contient seq_project + 16 tracks (64 steps × 4 voix) en .bss.

Pool p-lock : seq_plock_entry_t s_pool[SEQ_MAX_TRACKS * 64 * 24] (24 576 entrées × 3 octets ≈ 73 KiB). Monotone, sans GC.

Runner : tableaux s_plock_state[24], s_note_state[16][4], file s_on/off_events[64] – dimensionnés par track/voice.

LED bridge : caches CCM pour hold (hold_slots[16], hold_cart_params[SEQ_LED_BRIDGE_MAX_CART_PARAMS]), buffer d’édition 24 entrées.

Risque OOB : si pl_ref.offset corrompu, seq_reader_plock_iter_next accède seq_plock_pool_get(absolute,0) ; la fonction retourne NULL si hors bornes mais l’appelant ne journalise pas – silencieux.

Coût hot path :

    Runner : boucle 16 tracks × 4 voix + iteration p-lock (O(nb p-locks)). Inline, pas d’allocation.

LED bridge : pour chaque hold, collecte/commit (O(24)) + recomputation flags ; path UI (60 Hz) acceptable.

        Pool operations : simple copie mémoire, mais leaks → saturation long terme.

6) Fiabilité / gels (freeze après quelques minutes)

Hypothèses classées du plus probable au moins probable :

    Saturation du pool p-lock – chaque variation de nombre de p-locks alloue un segment irréversible ; absence de seq_plock_pool_reset côté firmware. Après nombreuses éditions/chargements, seq_plock_pool_alloc retourne -1, l’UI rollbacke mais laisse mutated_track partiellement mis à jour (warnings) → risque d’état incohérent / freeze si boucle UI/runner tourne sur un step reverti.

    À surveiller : logs [seq_led_bridge] p-lock commit failed, check seq_plock_pool_used() dans un debugger.

Course UI ↔ runner – seq_led_bridge_apply_* modifie les steps en place (hors staging hold) pendant que le runner lit les mêmes structures via seq_reader_peek_step. Sans mutex/critère hot-cold, le runner peut lire un step partiellement mis à jour (flags/voices non cohérents) → comportement indéfini, possible freeze si drapeaux active/automation divergents.

    Observer : freeze corrélé à tweaks simultanés et tick ; activer instrumentation pour vérifier.

IDs cart tronqués – l’envoi d’IDs 0x40–0xFF peut provoquer des accès hors plage cartouche (selon protocole), entraînant des réponses inattendues côté cart DSP pouvant bloquer l’échange (symptôme “p-lock cart cassés”).

    Vérifier sur bus cart si les IDs correspondent aux attentes.

Reader re-entry – s_plock_iter_state est global : si deux itérations sont ouvertes simultanément (ex. UI LED snapshot + runner), l’état est corrompu. Aujourd’hui le runner n’utilise qu’une instance, mais toute future évolution (multi-thread) pourrait provoquer des corruptions entraînant des boucles infinies / freeze.

        Confirmer qu’aucune autre tâche n’ouvre l’iter en parallèle.

Points de validation : surveiller s_used du pool, instrumenter seq_led_bridge pour détecter les erreurs, inspecter bus UART cart.
7) Dette technique & priorités

Ce qui est bien

    Structures pl_ref packées, macros #pragma GCC poison pour interdire le legacy.

Séparation Reader-only (handles) et alias hot/cold déjà amorcée.

UI backend centralise le routage et déclenche seq_led_bridge uniquement quand hold actif.

À améliorer

    Encoder les p-lock cart en 16 bits (pool + runner + bridge) pour respecter les specs cart link.

Introduire une gestion mémoire (reset/compaction) du pool et tracer son occupation runtime.

Formaliser la séparation hot/cold : interdire seq_project d’inclure le reader, fournir une API froide pour PLK2, éviter les alias directs dans les modules persistance.

Sécuriser les écritures UI vs runner (lock, copie atomique, staging global).

Priorités
Priorité	Travaux	Critères d’acceptation
P0	Corriger le stockage 16 bits des p-locks cart (bridge, pool, runner, codec).	seq_led_bridge/seq_project stockent l’ID complet, seq_engine_runner renvoie l’ID d’origine, tests cart p-lock passent, aucune valeur tronquée.
P0	Ajouter une stratégie de reset/GC du pool (clear track/pattern, load).	seq_plock_pool_reset() appelé lors des opérations d’effacement/chargement ; occupation max bornée ; plus de saturations silencieuses.
P1	Encapsuler l’accès concurrent step UI/runner (staging ou section critique).	Runner lit des snapshots cohérents (copie locale ou lock) ; plus de mutations directes pendant TICK.
P1	Extraire la dépendance seq_project.c → reader et préparer de vraies structures cold.	Pas d’inclusion reader dans la persistence ; interface dédiée pour les PLK2 (encode/décoder).
P2	Durcir les assertions (pl_ref, offsets, pool) et tracer les erreurs.	Assertions/LOG systématiques sur offset+count et parameter_id; instrumentation sur pool utilisé.
P2	Rendre seq_reader_plock_iter réentrant (state local) pour futurs parallélismes.	Pas de state global ; tests host confirment plusieurs iters simultanés.
8) Annexes
Invariants par module
Module	Invariant attendu	Enforcement
seq_model_step_t	pl_ref.count ≤ 24, offset valide, flags cohérents avec voix	count borné par _Static_assert, mais aucun check runtime ; flags recalculés manuellement.
seq_plock_pool	s_used ≤ capacity, allocations contiguës	Pas de garde haute ni de reset auto ; seq_plock_pool_get retourne NULL si dépassement.
seq_led_bridge	Buffer ≤ 24, rollback sur erreur, staging pour hold	Vérifie buffer->count avant append ; rollback mais pas de logging global ; modifie directement le step hors hold.
seq_engine_runner	s_on/off_count < 64, notes OFF jamais perdues	Capacité fixée, dépassement = drop silencieux ; _runner_reset_notes lors de transport stop.
cart_link	param_id < 512	Vérification en entrée ; ignore les IDs supérieurs (p-locks tronqués deviennent “valides” si <512).
Glossaire ID/flags p-locks
Symbole	Signification	Source
PL_INT_* (0x00–0x17)	Paramètres internes par voix (Note/Vel/Len/Mic) + offsets globaux	
k_seq_led_bridge_pl_flag_domain_cart / SEQ_READER_PL_FLAG_DOMAIN_CART (bit 0)	Marqueur “ID déjà cart”	
k_seq_led_bridge_pl_flag_signed / SEQ_READER_PL_FLAG_SIGNED (bit 1)	Valeur signée encodée via pl_u8_from_s8	
VOICE_SHIFT (bits 2-3)	Index voix (0..3) encodé dans flags	
Fichiers inspectés (principaux)

    Documentation : README.md, SEQ_BEHAVIOR.md, docs/ARCHITECTURE_FR.md.

    Sequencer core : core/seq/seq_model.[ch], seq_project.[ch], seq_runtime.[ch], runtime/seq_runtime_cold.c, seq_plock_pool.[ch], seq_plock_ids.h, seq_reader.[ch].

    Apps/UI : apps/seq_led_bridge.c, apps/seq_engine_runner.c, ui/ui_backend.c.

    Cart : core/cart_link.c, core/spec/cart_spec_types.h.

Fichiers non inspectés (hors périmètre immédiat) : drivers bas niveau (drivers/), pile MIDI (midi/), majority tests/stubs, modules ARP/keyboard, scripts build.
Questions ouvertes

    Les cartouches attendent-elles effectivement des IDs 0..511 (16 bits) côté protocole UART ? Confirmation nécessaire pour valider l’impact du tronquage.

    Existe-t-il un mécanisme externe (hors code analysé) qui reset le pool (ex. reboot) avant saturation ? Si oui, à quelle fréquence ?

    Souhaitez-vous une stratégie spécifique pour synchroniser les écritures UI (ex. double-buffering des steps) ou un lock léger suffit-il dans ce contexte temps réel ?


    Table des dépendances (UI / apps / core / cart)
Couche	Fichier(s) représentatifs	Dépend de	Preuve
UI	ui/ui_backend.c	Pont LED & runner (seq_led_bridge.h, seq_engine_runner.h), lien cart (cart_link.h), gestion MIDI/UI locales	#include en tête de fichier.
Apps (LED bridge)	apps/seq_led_bridge.c	Lecteur hot (seq_reader.h), accès modèle/projet, pool p-lock	Inclusions directes vers core/seq/*.
Apps (runner)	apps/seq_engine_runner.c	Cart link, registres cart, accès reader & IDs p-lock	Inclusions croisées vers cart_link.h, core/seq/*.
Core / Reader	core/seq/reader/seq_reader.c	Runtime/projet séquenceur, pool p-lock	Inclusions vers seq_runtime.h, seq_project.h, seq_plock_pool.h.
Cart	core/cart_link.c	Registre cart, runtime/projet séquenceur	Inclusions vers cart_registry.h, seq_project.h, seq_runtime.h.
Graphes Mermaid
(a) Inclusions majeures

graph TD
  UIBackend["ui_backend.c"] --> SeqLedBridge["apps/seq_led_bridge.c"]
  UIBackend --> Runner["apps/seq_engine_runner.c"]
  SeqLedBridge --> SeqReader["core/seq/reader/seq_reader.c"]
  SeqLedBridge --> SeqModel["core/seq/seq_model.c"]
  SeqLedBridge --> PlockPool["core/seq/seq_plock_pool.c"]
  Runner --> SeqReader
  Runner --> CartLink["core/cart_link.c"]
  SeqReader --> SeqProject["core/seq/seq_project.h"]
  SeqReader --> SeqRuntime["core/seq/seq_runtime.h"]
  CartLink --> SeqProject
  CartLink --> SeqRuntime

(b) Appels haut niveau

graph LR
  UIHold["UI hold (ui_backend)"] --> BridgeApply["seq_led_bridge_apply_*"]
  BridgeApply --> ModelCommit["seq_model_step_set_plocks_pooled"]
  ModelCommit --> PoolAlloc["seq_plock_pool_alloc"]
  RunnerTick["Tick runner"] --> ReaderPeek["seq_reader_peek_step / pl_next"]
  ReaderPeek --> RunnerCart["runner cart plock"]
  RunnerCart --> CartDispatch["cart_link_param_changed"]

Preuves localisées des risques P0
1. Encodage cart tronqué à 8 bits (bridge → reader → runner → cart link)

    L’UI tronque l’ID cartouche à 8 bits (0x40 + ID & 0xFF, clamp 0xFF) avant de le stocker dans le pool.

Lors de la lecture, le reader réémet le même octet (cart domain → param_id = id) vers le runner.

Le runner transfère ce même uint16_t param_id (issu de l’octet tronqué) à cart_link_param_changed.

cart_link_param_changed attend pourtant un ID < CART_LINK_MAX_DEST_ID (512) et pilote un shadow à 512 entrées.

    La spec cart XVA1 publie des IDs ≥285 (ex. SawTune=285), donc >255 et incompatibles avec l’encodage 8 bits.

Conclusion : toute valeur ≥256 est rabattue sur 0xFF avant envoi → perte d’ID cart 16 bits (P0 confirmé).
2. Itérateur p-locks global non réentrant

    L’état d’itération est un unique static seq_reader_plock_iter_state_t s_plock_iter_state.

    seq_reader_plock_iter_open réinitialise ce global puis retourne son adresse dans l’itérateur utilisateur.

    seq_reader_plock_iter_next caste _opaque vers ce même objet global.

Conclusion : un seul itérateur peut être actif ; un second appel simultané écraserait l’état (P0 confirmé).
3. Mutation concurrente des steps par l’UI pendant la lecture runner

    En mode hold, l’UI récupère un pointeur direct sur track->steps[...] via _step_from_page() (alias g.track).

    seq_led_bridge_apply_plock_param édite en place offsets, voix et p-locks, puis commit dans le pool.

    seq_led_bridge_apply_cart_param suit la même voie pour les p-locks cart.

    Le runner, dans _runner_step_ctx_prepare, capture simultanément un pointeur constant sur le même step via seq_reader_peek_step.

    Aucun verrouillage ni copie : lecture et écriture se font sur la même structure pendant le tick.

Conclusion : course UI ↔ runner sur les steps actifs (P0 confirmé).
4. Pool p-lock monotone sans reset/GC

    L’allocation est strictement monotone : s_used += n, jamais décrémenté.

seq_model_step_set_plocks_pooled n’alloue un nouveau bloc qu’en cas de variation de count, sans jamais libérer l’ancien.

Aucun seq_plock_pool_reset n’est invoqué côté firmware (core/, apps/, ui/) — seules les cibles de test l’appellent.

Conclusion : le pool se remplit irréversiblement en usage réel → risque d’OOM bloquant (P0 confirmé).
Checklists d’invariants à valider à l’exécution

    pl_ref.count <= SEQ_MAX_PLOCKS_PER_STEP et offset+count <= pool_used : contrôler juste après seq_model_step_set_plocks_pooled (usage UI/runner). SEQ_MAX_PLOCKS_PER_STEP vaut 24.

Vérifier avant encodage cart : parameter_id < CART_LINK_MAX_DEST_ID et propager l’ID complet (instrumentation possible dans seq_led_bridge_apply_cart_param).

    S’assurer qu’un seul itérateur seq_reader_plock_iter_* est ouvert pendant le tick (_opaque = &s_plock_iter_state).

    Surveiller qu’aucune écriture de step ne survient durant seq_engine_runner_on_clock_step (UI hold actif). Contrôler via instrumentation autour des accès g.track->steps et seq_reader_peek_step.

Mesures (BSS et tailles critiques)

    Mesures directes (sizeof) :
    – seq_model_step_t = 32 o (→ 64 steps ≈ 2 048 o)
    – seq_model_track_t = 2 064 o
    – seq_project_t = 73 128 o
    – seq_plock_entry_t = 3 o
    – SEQ_MAX_TRACKS = 16, SEQ_STEPS_PER_TRACK = 64, SEQ_MAX_PLOCKS_PER_STEP = 24

Pool p-lock : 16×64×24 = 24 576 entrées → ~73 728 o. Avec 8 tracks actives, le besoin tombe à 12 288 entrées (~36 864 o).

Runtime séquenceur (g_seq_runtime) : seq_project_t (73 128 o) + 16 × seq_model_track_t (33 024 o) ≈ 106 152 o en .bss pour 16 tracks. En configuration 8 tracks, la part « track » tombe à ~16 512 o (total ≈ 89 640 o).

    Un step individuel : 32 o → à surveiller pour snapshots/temp buffers.

Expériences de reproduction (sans code)

    Épuisement du pool

        Enchaîner des holds multi-steps et alterner entre 0, 1, puis plusieurs p-locks sur chaque step (variation de count → nouvelles allocations).

        Charger/supprimer des patterns pour remplir des segments morts.

        Observer SEQ_LED_BRIDGE_WARN("p-lock commit failed…") et seq_plock_pool_used() via logs debug.

    Conflit d’itérateur

        Déclencher une lecture p-lock côté UI (ex. future LED snapshot utilisant seq_reader_plock_iter_*) pendant que le runner tourne.

        Forcer une double ouverture (UI + tâche debug) pour vérifier corruption _opaque.

    Lecture pendant mutation

        Maintenir un step en hold, lancer le séquenceur (PLAY), puis tweaker rapidement note/offset (UI hold) pendant le passage du playhead.

        Tracer seq_led_bridge_apply_plock_param et _runner_step_ctx_process_plocks pour capturer états intermédiaires incohérents.

Priorisation finale & critères d’acceptation
Priorité	Risque	Critère observable attendu
P0	IDs cart tronqués	cart_link_param_changed reçoit l’ID 16 bits exact (logs ou shadow), et les params >255 sont correctement rejoués.
P0	Itérateur global non réentrant	Garantir un itérateur réentrant (ex. instance locale) ou prouver qu’un seul appelant peut s’exécuter (assert/runtime check).
P0	Mutation concurrente steps	Bloquer/ordonner les mutations (hot/cold séparé) ou fournir un mécanisme de snapshot atomique ; vérifier que le runner voit un état cohérent lors de tweaks.
P0	Pool monotone sans reset	Introduire reset/GC (clear track/pattern/load) ou recycle d’anciens blocs. Accepter quand seq_plock_pool_used() reste borné après cycles d’édition.
P1	Monitoring insuffisant pool/IDs	Ajouter compteurs/logs runtime (pool watermarks, ID invalides). Acceptance : visibilité dans logs UI (sans freeze).
P2	Conventions UI/cart (shadow)	Documentation/alertes pour UI shadow si hold actif. Acceptance : warning clair côté UI sans impact temps réel.
Mini-checklist (actions préliminaires)

    IDs cart >255 confirmés : SawTune=285, Drift=260, etc.

    Logs à surveiller : SEQ_LED_BRIDGE_WARN(...commit failed...) pour OOM, BRICK_DEBUG_PLOCK_LOG("cart", param_id, …) pour suivre l’ID réellement envoyé.

    Points UI ↔ tick : ui_backend_param_changed et ui_backend_handle_ui déclenchent seq_led_bridge_apply_* en plein hold, côté runner lecture dans _runner_step_ctx_prepare/_process_plocks.

    Itérateur p-locks : unique site global (seq_reader_plock_iter_*). Vérifier toute nouvelle utilisation.

Aucun test automatisé exécuté (audit statique uniquement).

