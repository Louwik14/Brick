#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${REPO_ROOT}"

ELF="build/ch.elf"
MAP="build/ch.map"
TOP_TXT="build/bss_top50.txt"
TOP_JSON="build/bss_top.json"
REPORT="build/BSS_DIAG.md"

if [[ ! -f "${ELF}" ]]; then
  echo "[bss_top] ELF manquant : ${ELF}. Construire le firmware (make all)." >&2
  exit 2
fi

if [[ ! -f "${MAP}" ]]; then
  echo "[bss_top] Linker map absente (${MAP}). Relancer la construction." >&2
fi

mkdir -p build

arm-none-eabi-nm -S --size-sort "${ELF}" \
  | awk '$3 ~ /^[bB]$/' \
  | sort -k2,2nr \
  | head -50 | tee "${TOP_TXT}"

python3 - <<'PY'
import json
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

repo_root = Path.cwd()
elf = Path("build/ch.elf")
map_path = Path("build/ch.map")
top_txt = Path("build/bss_top50.txt")
json_path = Path("build/bss_top.json")
report_path = Path("build/BSS_DIAG.md")

if not top_txt.exists():
    raise SystemExit("Liste top50 absente")

if not map_path.exists():
    raise SystemExit("Linker map introuvable, relancer la construction")

with top_txt.open("r", encoding="utf-8") as fh:
    top_lines = [line.strip() for line in fh if line.strip()]

size_proc = subprocess.run(["arm-none-eabi-size", str(elf)], capture_output=True, text=True, check=True)
size_lines = [line for line in size_proc.stdout.splitlines() if line.strip()]
if len(size_lines) < 2:
    raise SystemExit("Impossible de lire arm-none-eabi-size")
header = size_lines[0].split()
values = size_lines[1].split()
if "bss" not in header:
    raise SystemExit("Colonne bss absente")
bss_total = int(values[header.index("bss")])

object_map: dict[tuple[str, str], str] = {}
symbol_map: dict[str, set[str]] = {}

with map_path.open("r", encoding="utf-8", errors="ignore") as fh:
    current_object: str | None = None
    pending_symbol: str | None = None
    pending_addr: str | None = None
    for raw_line in fh:
        line = raw_line.rstrip("\n")
        stripped = line.lstrip()
        if not stripped:
            continue
        if stripped.startswith('.bss'):
            parts = stripped.split()
            section = parts[0]
            symbol = None
            if section.startswith('.bss.'):
                symbol = section.split('.', 2)[2]
            addr = None
            obj = None
            if len(parts) >= 3 and parts[1].startswith('0x'):
                addr = parts[1].upper()
            if len(parts) >= 4 and parts[-1].endswith('.o'):
                obj = parts[-1]
            if obj:
                current_object = obj
                if symbol and addr:
                    object_map[(addr, symbol)] = obj
                    symbol_map.setdefault(symbol, set()).add(obj)
                pending_symbol = None
                pending_addr = None
            else:
                pending_symbol = symbol
                pending_addr = addr
                current_object = None
            continue
        if stripped.startswith('*fill*'):
            continue
        if stripped.startswith('0x'):
            parts = stripped.split()
            addr = parts[0].upper()
            obj = None
            if len(parts) >= 3 and parts[-1].endswith('.o'):
                obj = parts[-1]
                current_object = obj
                if pending_symbol:
                    use_addr = pending_addr or addr
                    object_map[(use_addr.upper(), pending_symbol)] = obj
                    symbol_map.setdefault(pending_symbol, set()).add(obj)
                    pending_symbol = None
                    pending_addr = None
            symbol = None
            if len(parts) >= 2:
                candidate = parts[1]
                if candidate != '*fill*' and not candidate.endswith('.o') and not candidate.startswith('0x'):
                    symbol = candidate
            if symbol and current_object:
                object_map[(addr, symbol)] = current_object
                symbol_map.setdefault(symbol, set()).add(current_object)
            continue
        current_object = None
        pending_symbol = None
        pending_addr = None

symbol_re = re.compile(r"([A-Za-z0-9_./\\-]+\\.(?:c|h|cpp))")


def normalize_path(raw: str | None) -> str | None:
    if not raw:
        return None
    raw = raw.replace('\\', '/')
    candidate = Path(raw)
    if candidate.is_absolute():
        try:
            return str(candidate.relative_to(repo_root))
        except ValueError:
            return str(candidate)
    if (repo_root / candidate).exists():
        return str(candidate)
    return str(candidate)


def detect_source(obj: str | None) -> str | None:
    if not obj:
        return None
    obj_path = Path(obj)
    if not obj_path.is_absolute():
        obj_path = repo_root / obj_path
    if not obj_path.exists():
        return None
    commands = [
        ["arm-none-eabi-objdump", "--dwarf=info", str(obj_path)],
        ["arm-none-eabi-objdump", "--dwarf=decodedline", str(obj_path)],
    ]
    for cmd in commands:
        try:
            proc = subprocess.run(cmd, capture_output=True, text=True, check=False)
        except FileNotFoundError:
            return None
        text = proc.stdout
        for match in symbol_re.finditer(text):
            src = normalize_path(match.group(1))
            if src:
                return src
    return None


def hypothesis(symbol: str) -> str:
    low = symbol.lower()
    if "pool" in low:
        return "Pool statique à rebasculer sur alloc pool"
    if "shadow" in low:
        return "Shadow/cache UI à migrer"
    if "cache" in low:
        return "Cache statique à migrer vers pool"
    if "table" in low or "tbl" in low or "lut" in low:
        return "Table statique potentiellement migrable"
    if "buffer" in low or "buf" in low:
        return "Buffer massif à ré-héberger (pool/stream)"
    if "state" in low or "ctx" in low:
        return "Etat global à compacter"
    if "queue" in low or "fifo" in low:
        return "File/queue à basculer sur pool"
    if "runtime" in low:
        return "Runtime massif : vérifier granularité"
    return "Revue nécessaire"


def strategy(symbol: str) -> str:
    low = symbol.lower()
    if "pool" in low:
        return "Migrer le pool vers allocation dynamique contrôlée"
    if "shadow" in low:
        return "Réduire/muter le shadow UI (pool + invalidation ciblée)"
    if "cache" in low:
        return "Supprimer/migrer le cache vers un pool dynamique"
    if "table" in low or "lut" in low:
        return "Externaliser la table (flash) ou générer à la demande"
    if "buffer" in low or "buf" in low:
        return "Bascule vers pool partagé ou réduction dimension"
    if "state" in low or "ctx" in low:
        return "Compacter l'état ou le découper en structures pool"
    if "runtime" in low:
        return "Segmenter le runtime et charger à la demande"
    return "Auditer et planifier une migration vers pool"

entries = []
for rank, line in enumerate(top_lines, 1):
    parts = line.split()
    if len(parts) < 4:
        continue
    addr, size_hex, kind = parts[:3]
    symbol = " ".join(parts[3:])
    try:
        size_bytes = int(size_hex, 16)
    except ValueError:
        continue
    obj = object_map.get((addr.upper(), symbol))
    if not obj:
        possible = list(symbol_map.get(symbol, []))
        if len(possible) == 1:
            obj = possible[0]
    obj_rel = None
    if obj:
        obj_path = Path(obj)
        if not obj_path.is_absolute():
            obj_path = (repo_root / obj_path).resolve()
        if obj_path.exists():
            try:
                obj_rel = str(obj_path.relative_to(repo_root))
            except ValueError:
                obj_rel = str(obj_path)
        else:
            obj_rel = obj
    source = detect_source(obj_rel or obj)
    entry = {
        "rank": rank,
        "size_bytes": size_bytes,
        "symbol": symbol,
        "object": obj_rel,
        "source": source,
        "section": ".bss",
        "notes": hypothesis(symbol),
    }
    entries.append(entry)

json_data = {
    "generated_at": datetime.now(timezone.utc).isoformat(),
    "elf": str(elf),
    "entries": entries,
}
json_path.write_text(json.dumps(json_data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

summary_lines = ["## Résumé", ""]
summary_lines.append(f"- Taille .bss actuelle : {bss_total} octets (arm-none-eabi-size)")
for entry in entries[:3]:
    percent = (entry["size_bytes"] / bss_total * 100) if bss_total else 0
    target = entry["object"] or "?"
    summary_lines.append(f"- `{entry['symbol']}` occupe {entry['size_bytes']} B ({percent:.2f}% du total) via {target}")
if len(summary_lines) < 5:
    summary_lines.append("- Concentration élevée des allocations statiques dans les 10 premiers symboles")
summary_lines.append("")

lines = ["# Diagnostic .bss", ""]
lines.extend(summary_lines)
lines.append("## Top 10 `.bss` (table)")
lines.append("")
lines.append("| Rank | Size (bytes) | Symbol | Object | Source | Hypothèse |")
lines.append("| --- | --- | --- | --- | --- | --- |")
for entry in entries[:10]:
    obj_cell = entry["object"] or ""
    src_cell = entry["source"] or ""
    lines.append(f"| {entry['rank']} | {entry['size_bytes']} | `{entry['symbol']}` | {obj_cell} | {src_cell} | {entry['notes']} |")
lines.append("")
lines.append("## Suspects prioritaires (2–3)")
lines.append("")
for entry in entries[:3]:
    percent = (entry["size_bytes"] / bss_total * 100) if bss_total else 0
    target = entry["source"] or entry["object"] or entry["symbol"]
    lines.append(f"- `{entry['symbol']}` ({entry['size_bytes']} B, {percent:.2f}% .bss) – {entry['notes']}. Fichiers à toucher : {target}.")
lines.append("")
lines.append("## Proposition 9B (BSS-KILL ciblé)")
lines.append("")
for entry in entries[:3]:
    lines.append(f"- `{entry['symbol']}` → {strategy(entry['symbol'])} (cible : {entry['source'] or entry['object'] or 'analyse approfondie'})")
lines.append("")
lines.append("## Annexes")
lines.append("")
lines.append(f"- Linker map : `{map_path}`")
lines.append("- Reproduire :")
lines.append("  - `make bss-top`")
lines.append("  - `bash tools/bss_top.sh`")
lines.append("")

report_path.write_text("\n".join(lines), encoding="utf-8")
PY
