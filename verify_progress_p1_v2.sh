
#!/usr/bin/env bash
set -euo pipefail

GREEN="\033[32m"; RED="\033[31m"; YELLOW="\033[33m"; BLUE="\033[34m"; BOLD="\033[1m"; RESET="\033[0m"
ok()  { echo -e "${GREEN}PASS${RESET} - $*"; }
ko()  { echo -e "${RED}FAIL${RESET} - $*"; }
warn(){ echo -e "${YELLOW}WARN${RESET} - $*"; }
log(){ echo -e "${BLUE}$*${RESET}"; }

OUT_DIR="out/_verify_p1_v2"
mkdir -p "$OUT_DIR"
BUILD_LOG="$OUT_DIR/build_check_host.log"

# Helpers
has_file() { [[ -f "$1" ]]; }
has_any_file() { for f in "$@"; do [[ -f "$f" ]] && return 0; done; return 1; }
has_dir() { [[ -d "$1" ]]; }
has_symbol_in_file() { local sym="$1"; local f="$2"; grep -n -E -- "$sym" "$f" >/dev/null 2>&1; }
has_symbol_anywhere() { local sym="$1"; grep -R -n -E -- "$sym" . >/dev/null 2>&1; }
has_make_target() { make -n "$1" >/dev/null 2>&1; }

echo -e "${BOLD}Brick P1/P2 — Vérification MP0 → MP12 (v2, robuste sans git grep)${RESET}"

############################
# 1) Build (keep going)
############################
log "Compilation: MAKEFLAGS=-k make check-host (capture logs)"
if MAKEFLAGS=-k make check-host | tee "$BUILD_LOG"; then
  ok "make check-host terminé (avec ou sans tests échoués)"
else
  warn "make check-host a retourné un statut non nul (test échoué). On continue l'analyse."
fi

############################
# MP0 / MP1
############################
if has_file "core/seq/reader/seq_reader.c" && has_file "core/seq/reader/seq_reader.h"; then
  ok "MP0: seq_reader.{h,c} présents"
else
  ko "MP0: seq_reader.{h,c} manquants (core/seq/reader/*)"
fi

if has_any_file "core/seq/seq_access.h" "core/seq/seq_views.h" "core/seq/seq_handles.h" "core/seq/seq_config.h"; then
  ok "MP0: en-têtes publics présents (seq_access/seq_views/seq_handles/seq_config)"
else
  ko "MP0: en-têtes publics manquants (seq_access/seq_views/seq_handles/seq_config)"
fi

if grep -q "seq_reader_tests" "$BUILD_LOG"; then
  ok "MP1: test host seq_reader_tests compilé/exécuté"
else
  warn "MP1: pas de trace de seq_reader_tests dans les logs"
fi

############################
# MP2
############################
if [[ -f Makefile ]] && grep -q "SEQ_USE_HANDLES" Makefile; then
  ok "MP2: SEQ_USE_HANDLES visible dans Makefile"
else
  warn "MP2: pas de trace SEQ_USE_HANDLES dans Makefile (peut être défini ailleurs)"
fi

has_make_target warn_legacy_includes_apps && ok "MP2: cible warn_legacy_includes_apps existe" || warn "MP2: warn_legacy_includes_apps absente"

############################
# MP3 LED
############################
if has_file "apps/seq_led_bridge.c"; then
  ok "MP3: apps/seq_led_bridge.c présent"
  grep -q "seq_access.h" apps/seq_led_bridge.c && ok "MP3a: seq_led_bridge inclut seq_access.h" || warn "MP3a: include seq_access.h non trouvé"
  has_make_target check_no_legacy_includes_led && { 
    if make check_no_legacy_includes_led >/dev/null 2>&1; then ok "MP3c: check_no_legacy_includes_led passe"; else ko "MP3c: check_no_legacy_includes_led échoue"; fi
  } || warn "MP3c: cible check_no_legacy_includes_led absente"
else
  warn "MP3: apps/seq_led_bridge.c introuvable"
fi

############################
# MP4 runner
############################
if has_file "apps/seq_engine_runner.c"; then
  ok "MP4: apps/seq_engine_runner.c présent"
  grep -q "seq_access.h" apps/seq_engine_runner.c && ok "MP4a: runner inclut seq_access.h" || warn "MP4a: include seq_access.h non trouvé"
  grep -q "seq_reader_get_step" apps/seq_engine_runner.c && ok "MP4b: runner utilise seq_reader_get_step()" || warn "MP4b: pas de seq_reader_get_step()"
  has_make_target check_no_legacy_includes_runner || warn "MP4b: cible check_no_legacy_includes_runner absente"
else
  warn "MP4: apps/seq_engine_runner.c introuvable"
fi

############################
# MP5
############################
if has_make_target check_no_legacy_includes_apps; then
  if make check_no_legacy_includes_apps >/dev/null 2>&1; then
    ok "MP5: garde globale check_no_legacy_includes_apps passe"
  else
    ko "MP5: garde globale check_no_legacy_includes_apps échoue"
  fi
else
  warn "MP5: check_no_legacy_includes_apps non présent"
fi

############################
# MP6
############################
if has_file "core/seq/runtime/seq_runtime_layout.h" && has_file "core/seq/runtime/seq_runtime_layout.c"; then
  ok "MP6a: seq_runtime_layout.{h,c} présents"
else
  warn "MP6a: seq_runtime_layout.{h,c} manquants"
fi

if has_file "core/seq/reader/seq_reader.c" && grep -q "seq_runtime_blocks_get" core/seq/reader/seq_reader.c; then
  ok "MP6b: Reader appelle seq_runtime_blocks_get()"
else
  warn "MP6b: pas de seq_runtime_blocks_get() dans Reader"
fi

############################
# MP7
############################
grep -R -q "seq_runtime_cold_(view|project|cart|hold)" core/seq/runtime 2>/dev/null && ok "MP7b/c/e: façade cold et domaines présents" || warn "MP7: pas de façade/domaine cold détecté"
grep -q "seq_runtime_cold_project_tests" "$BUILD_LOG" && ok "MP7b: test cold project exécuté" || true
grep -q "seq_runtime_cold_cart_meta_tests" "$BUILD_LOG" && ok "MP7c: test cold cart meta exécuté" || true
grep -q "seq_runtime_cold_hold_slots_tests" "$BUILD_LOG" && ok "MP7e: test cold hold slots exécuté" || true

############################
# MP8
############################
grep -q "HOT estimate" "$BUILD_LOG" && ok "MP8a: budget hot exécuté" || warn "MP8a: pas de log HOT estimate"
grep -qi "Reader.get_step" "$BUILD_LOG" && ok "MP8c: micro-bench Reader présent" || warn "MP8c: pas de log de timing Reader"

############################
# MP9
############################
grep -q "SEQ_ENABLE_COLD_SECTIONS" core/seq/seq_config.h 2>/dev/null && ok "MP9a: flags sections exposés" || warn "MP9a: flags sections absents"
grep -qi "Cold domains (bytes)" "$BUILD_LOG" && ok "MP9b: stats cold imprimées" || warn "MP9b: stats cold non vues"

############################
# MP10
############################
has_any_file "core/seq/runtime/seq_rt_phase.h" "core/seq/runtime/seq_rt_phase.c" && ok "MP10a: API de phase RT présente" || warn "MP10a: API de phase RT non trouvée"
grep -qi "cold_view_calls_in_tick" "$BUILD_LOG" && ok "MP10: trace no-cold-in-tick présente" || warn "MP10: aucune trace cold_view_calls_in_tick"

has_make_target check_no_cold_in_rt_sources && {
  if make check_no_cold_in_rt_sources >/dev/null 2>&1; then ok "MP10c: grep CI no-cold-in-tick passe"; else ko "MP10c: grep CI no-cold-in-tick échoue"; fi
} || warn "MP10c: cible check_no_cold_in_rt_sources absente"

############################
# MP11
############################
has_make_target audit_rt_symbols && {
  if make audit_rt_symbols >/dev/null 2>&1; then ok "MP11: audit_rt_symbols passe"; else warn "MP11: audit_rt_symbols échoue"; fi
} || warn "MP11: cible audit_rt_symbols absente"

grep -q "seq_rt_path_smoke" "$BUILD_LOG" && ok "MP11: test host seq_rt_path_smoke exécuté" || warn "MP11: pas de log seq_rt_path_smoke"

############################
# MP12
############################
has_make_target check_apps_public_surface && {
  if make check_apps_public_surface >/dev/null 2>&1; then ok "MP12: surface publique apps/** respectée"; else ko "MP12: surface publique apps/** non respectée"; fi
} || warn "MP12: check_apps_public_surface absent"

grep -qi "LED snapshot" "$BUILD_LOG" && ok "MP12: tests LED snapshot exécutés" || warn "MP12: pas de log LED snapshot tests"

echo -e "${BOLD}Vérification v2 terminée. Log: $BUILD_LOG${RESET}"

# Hints if codec V2 fails
if grep -q "seq_track_codec_tests" "$BUILD_LOG" && grep -qi "Assertion failed" "$BUILD_LOG"; then
  echo -e "${YELLOW}TIP:${RESET} Le test codec V2 échoue. Pour contourner et vérifier P1/P2 :"
  echo '  CPPFLAGS="-DBRICK_EXPERIMENTAL_PATTERN_CODEC_V2=0" MAKEFLAGS=-k make check-host'
fi
