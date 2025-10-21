
#!/usr/bin/env bash
set -euo pipefail

# Colors
GREEN="\033[32m"; RED="\033[31m"; YELLOW="\033[33m"; BLUE="\033[34m"; BOLD="\033[1m"; RESET="\033[0m"

log() { echo -e "${BLUE}$*${RESET}"; }
ok()  { echo -e "${GREEN}PASS${RESET} - $*"; }
ko()  { echo -e "${RED}FAIL${RESET} - $*"; }
warn(){ echo -e "${YELLOW}WARN${RESET} - $*"; }

# Where to store logs
OUT_DIR="out/_verify_p1"
mkdir -p "$OUT_DIR"
BUILD_LOG="$OUT_DIR/build_check_host.log"

echo -e "${BOLD}Brick P1/P2 — Vérification MP0 → MP12 (build+greps)${RESET}"
echo "Logs: $BUILD_LOG"

# Helper: test presence of a symbol/file/pattern
has_pattern() {
  local pattern="$1"; shift
  if git grep -n -- "$pattern" -- "$@" >/dev/null 2>&1; then
    return 0
  else
    return 1
  fi
}

# Helper: check that make target exists (best-effort via -n and grep)
has_make_target() {
  local target="$1"
  if make -n "$target" >/dev/null 2>&1; then
    return 0
  else
    return 1
  fi
}

########################################
# 1) Build host once and capture output
########################################
log "make check-host (capture sortis console)"
if make check-host | tee "$BUILD_LOG"; then
  ok "make check-host a tourné sans erreur"
else
  ko "make check-host a échoué — continue avec les checks statiques"
fi

########################################
# MP0/MP1 — Façade Reader + tests host
########################################
if has_pattern "core/seq/reader/seq_reader.h" && has_pattern "core/seq/reader/seq_reader.c"; then
  ok "MP0: fichiers seq_reader.{h,c} présents"
else
  ko "MP0: fichiers seq_reader.{h,c} manquants"
fi

if has_pattern "core/seq/seq_access.h" && has_pattern "core/seq/seq_views.h" && has_pattern "core/seq/seq_handles.h" && has_pattern "core/seq/seq_config.h"; then
  ok "MP0: en-têtes publics Reader/Handles/Views présents"
else
  ko "MP0: en-têtes publics manquants (seq_access/seq_views/seq_handles/seq_config)"
fi

if grep -q "seq_reader_tests" "$BUILD_LOG"; then
  ok "MP1: test host seq_reader_tests a été construit/exécuté"
else
  warn "MP1: impossible de confirmer seq_reader_tests dans le log build"
fi

########################################
# MP2 — Plomberie build handles (opt-in)
########################################
if has_pattern "SEQ_USE_HANDLES" "Makefile"; then
  ok "MP2: injection SEQ_USE_HANDLES détectée dans Makefile"
else
  warn "MP2: pas de trace SEQ_USE_HANDLES dans Makefile"
fi

if has_make_target "warn_legacy_includes_apps"; then
  ok "MP2: cible warn_legacy_includes_apps existe"
else
  warn "MP2: cible warn_legacy_includes_apps introuvable"
fi

########################################
# MP3 — LED bridge vers Reader + garde
########################################
if has_pattern "apps/seq_led_bridge.c"; then
  if has_pattern "core/seq/seq_access.h" "apps/seq_led_bridge.c"; then
    ok "MP3a: seq_led_bridge inclut seq_access.h"
  else
    warn "MP3a: seq_led_bridge ne semble pas inclure seq_access.h"
  fi
  if has_pattern "check_no_legacy_includes_led" "Makefile"; then
    ok "MP3c: cible check_no_legacy_includes_led définie"
    if make check_no_legacy_includes_led >/dev/null 2>&1; then
      ok "MP3c: check_no_legacy_includes_led passe"
    else
      ko "MP3c: check_no_legacy_includes_led échoue"
    fi
  else
    warn "MP3c: cible check_no_legacy_includes_led absente"
  fi
else
  warn "MP3: apps/seq_led_bridge.c introuvable"
fi

########################################
# MP4 — Runner opt-in Reader partiel
########################################
if has_pattern "apps/seq_engine_runner.c"; then
  if has_pattern "core/seq/seq_access.h" "apps/seq_engine_runner.c"; then
    ok "MP4a: runner pré-câblé vers seq_access.h"
  else
    warn "MP4a: runner sans include seq_access.h"
  fi
  if has_pattern "seq_reader_get_step" "apps/seq_engine_runner.c"; then
    ok "MP4b: runner consomme seq_reader_get_step sous opt-in"
  else
    warn "MP4b: pas de référence à seq_reader_get_step dans runner"
  fi
  if has_pattern "check_no_legacy_includes_runner" "Makefile"; then
    ok "MP4b: cible check_no_legacy_includes_runner définie"
  else
    warn "MP4b: cible check_no_legacy_includes_runner absente"
  fi
else
  warn "MP4: apps/seq_engine_runner.c introuvable"
fi

########################################
# MP5 — Flip global handles + garde apps/**
########################################
if has_make_target "check_no_legacy_includes_apps"; then
  if make check_no_legacy_includes_apps >/dev/null 2>&1; then
    ok "MP5: garde globale check_no_legacy_includes_apps passe"
  else
    ko "MP5: garde globale check_no_legacy_includes_apps échoue"
  fi
else
  warn "MP5: check_no_legacy_includes_apps non présent"
fi

########################################
# MP6 — Préparation barrière hot/cold
########################################
if has_pattern "core/seq/runtime/seq_runtime_layout.h" && has_pattern "core/seq/runtime/seq_runtime_layout.c"; then
  ok "MP6a: fichiers seq_runtime_layout.{h,c} présents"
else
  warn "MP6a: seq_runtime_layout.{h,c} manquants"
fi

if has_pattern "seq_runtime_blocks_get" "core/seq/reader/seq_reader.c"; then
  ok "MP6b: Reader utilise seq_runtime_blocks_get()"
else
  warn "MP6b: pas de trace de seq_runtime_blocks_get() dans Reader"
fi

########################################
# MP7 — Vues cold (PROJECT/CART_META/HOLD_SLOTS)
########################################
if has_pattern "core/seq/runtime/seq_runtime_cold.h" || has_pattern "core/seq/runtime/seq_runtime_cold.h" "core/seq/runtime/"; then
  : # no-op
fi

if grep -Eq "seq_runtime_cold_(view|project|cart|hold)" -R core/seq/runtime 2>/dev/null; then
  ok "MP7b/c/e: façade cold et domaines présents"
else
  warn "MP7: pas de façade/domaine cold détecté"
fi

if grep -q "seq_runtime_cold_project_tests" "$BUILD_LOG"; then
  ok "MP7b: test host cold project exécuté"
fi
if grep -q "seq_runtime_cold_cart_meta_tests" "$BUILD_LOG"; then
  ok "MP7c: test host cold cart meta exécuté"
fi
if grep -q "seq_runtime_cold_hold_slots_tests" "$BUILD_LOG"; then
  ok "MP7e: test host cold hold slots exécuté"
fi

########################################
# MP8 — Budget hot + micro-bench
########################################
if grep -q "HOT estimate" "$BUILD_LOG"; then
  ok "MP8a: garde budget hot (host) exécutée"
else
  warn "MP8a: pas de log HOT estimate dans build"
fi

if grep -qi "Reader.get_step" "$BUILD_LOG"; then
  ok "MP8c: micro-bench Reader timing présent"
else
  warn "MP8c: pas de log de timing Reader"
fi

########################################
# MP9 — Plomberie sections + stats cold
########################################
if has_pattern "SEQ_ENABLE_COLD_SECTIONS" "core/seq/seq_config.h"; then
  ok "MP9a: flags sections hot/cold exposés dans seq_config.h"
else
  warn "MP9a: flags sections absents"
fi

if grep -qi "Cold domains (bytes)" "$BUILD_LOG"; then
  ok "MP9b: stats cold imprimées dans check-host"
else
  warn "MP9b: stats cold non vues dans logs"
fi

########################################
# MP10 — No-cold-in-tick (host CI)
########################################
if has_pattern "core/seq/runtime/seq_rt_phase.h" && has_pattern "core/seq/runtime/seq_rt_phase.c" || has_pattern "core/seq/runtime/seq_rt_phase.c"; then
  ok "MP10a: API de phase RT présente"
else
  warn "MP10a: API de phase RT non trouvée"
fi

if grep -qi "cold_view_calls_in_tick" "$BUILD_LOG"; then
  ok "MP10: trace no-cold-in-tick présente dans make check-host"
else
  warn "MP10: aucune trace cold_view_calls_in_tick dans logs"
fi

if has_make_target "check_no_cold_in_rt_sources"; then
  if make check_no_cold_in_rt_sources >/dev/null 2>&1; then
    ok "MP10c: grep CI no-cold-in-tick passe"
  else
    ko "MP10c: grep CI no-cold-in-tick échoue"
  fi
else
  warn "MP10c: cible check_no_cold_in_rt_sources absente"
fi

########################################
# MP11 — Audit symboles RT + smoke test
########################################
if has_make_target "audit_rt_symbols"; then
  if make audit_rt_symbols >/dev/null 2>&1; then
    ok "MP11: audit_rt_symbols passe"
  else
    warn "MP11: audit_rt_symbols échoue"
  fi
else
  warn "MP11: cible audit_rt_symbols absente"
fi

if grep -q "seq_rt_path_smoke" "$BUILD_LOG"; then
  ok "MP11: test host seq_rt_path_smoke exécuté"
else
  warn "MP11: pas de log seq_rt_path_smoke"
fi

########################################
# MP12 — Surface apps/** + LED snapshot
########################################
if has_make_target "check_apps_public_surface"; then
  if make check_apps_public_surface >/dev/null 2>&1; then
    ok "MP12: surface publique apps/** respectée"
  else
    ko "MP12: surface publique apps/** non respectée"
  fi
else
  warn "MP12: check_apps_public_surface absent"
fi

if grep -qi "LED snapshot" "$BUILD_LOG"; then
  ok "MP12: tests LED snapshot exécutés (diffs: 0 attendu)"
else
  warn "MP12: pas de log LED snapshot tests"
fi

echo -e "${BOLD}Vérification terminée. Consulte $BUILD_LOG pour les détails.${RESET}"
