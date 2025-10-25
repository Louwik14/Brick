#!/usr/bin/env bash
set -euo pipefail
ELF=${1:-build/ch.elf}
[[ -f "$ELF" ]] || { echo "❌ ELF introuvable: $ELF"; exit 2; }
line=$(size "$ELF" | tail -n1)
read -r text data bss dec hex _ <<<"$line"
echo "text=$text data=$data bss=$bss dec=$dec hex=$hex"
# marge souple: 80 KiB; ajuste si besoin après mesures réelles
max_bss=$((80*1024))
if (( bss > max_bss )); then
  echo "⚠️  bss>$max_bss: vérifier qu’aucun gros buffer per-step n’a réapparu"
  exit 1
fi
echo "OK"
