
#!/usr/bin/env bash
set -euo pipefail
GREEN="\033[32m"; RED="\033[31m"; YELLOW="\033[33m"; BLUE="\033[34m"; BOLD="\033[1m"; RESET="\033[0m"
ok(){ echo -e "${GREEN}PASS${RESET} - $*"; }
warn(){ echo -e "${YELLOW}WARN${RESET} - $*"; }
ko(){ echo -e "${RED}FAIL${RESET} - $*"; }

echo -e "${BOLD}Brick P1/P2 — Vérification statique MP0 → MP12 (greps uniquement)${RESET}"

check_grep() {
  local pattern="$1"; shift
  local paths=("$@")
  if git grep -n -- "$pattern" -- "${paths[@]}" >/dev/null 2>&1; then
    ok "Pattern trouvé: $pattern"
  else
    ko "Pattern manquant: $pattern"
  fi
}

# MP0/1
check_grep "core/seq/reader/seq_reader.h" .
check_grep "core/seq/reader/seq_reader.c" .
check_grep "core/seq/seq_access.h" .
check_grep "tests/seq_reader_tests.c" .

# MP2
check_grep "SEQ_USE_HANDLES" Makefile

# MP3 LED + garde
check_grep "apps/seq_led_bridge.c" .
check_grep "core/seq/seq_access.h" apps/seq_led_bridge.c
check_grep "check_no_legacy_includes_led" Makefile

# MP4 runner
check_grep "apps/seq_engine_runner.c" .
check_grep "seq_reader_get_step" apps/seq_engine_runner.c

# MP5
check_grep "check_no_legacy_includes_apps" Makefile

# MP6 layout
check_grep "core/seq/runtime/seq_runtime_layout.h" .
check_grep "seq_runtime_blocks_get" core/seq/reader/seq_reader.c

# MP7 cold views
check_grep "seq_runtime_cold_view" core/seq/runtime

# MP8 hot budget
check_grep "tests/seq_hot_budget_tests.c" .

# MP9 sections + cold stats
check_grep "SEQ_ENABLE_COLD_SECTIONS" core/seq/seq_config.h
check_grep "seq_runtime_cold_stats" tests

# MP10 phase API + grep CI
check_grep "core/seq/runtime/seq_rt_phase.h" .
check_grep "check_no_cold_in_rt_sources" Makefile

# MP11
check_grep "audit_rt_symbols" Makefile
check_grep "tests/seq_rt_path_smoke.c" .

# MP12 public surface + LED snapshot
check_grep "check_apps_public_surface" Makefile
check_grep "tests/seq_led_snapshot_tests.c" .

echo -e "${BOLD}Greps terminés.${RESET}"
