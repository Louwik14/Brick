#!/usr/bin/env python3
"""Builds key UI/LED modules with arm-none-eabi and reports RAM symbols."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = ROOT / "build" / "ui_ram_audit"

DEFAULT_SOURCES = [
    "apps/seq_led_bridge.c",
    "ui/ui_led_backend.c",
    "drivers/drv_leds_addr.c",
    "drivers/drv_display.c",
    "core/seq/seq_project.c",
    "core/seq/seq_runtime.c",
]


def run(cmd: list[str], *, cwd: Path) -> None:
    subprocess.run(cmd, cwd=cwd, check=True)


def compile_objects(gcc: str, sources: list[str]) -> list[Path]:
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    objects: list[Path] = []
    common_flags = [
        "-c",
        "-mcpu=cortex-m4",
        "-mthumb",
        "-Os",
        "-ffunction-sections",
        "-fdata-sections",
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Wno-unused-function",
        "-Itests/stubs",
        "-Iui",
        "-Iapps",
        "-Idrivers",
        "-Imidi",
        "-Icart",
        "-Iboard",
        "-Icore",
        "-Icore/seq",
        "-I.",
    ]
    for src in sources:
        src_path = ROOT / src
        obj = BUILD_DIR / (src_path.stem + ".o")
        cmd = [gcc, *common_flags, str(src_path), "-o", str(obj)]
        run(cmd, cwd=ROOT)
        objects.append(obj)
    return objects


def collect_symbols(nm: str, objects: list[Path]) -> tuple[dict[str, int], dict[str, str], set[str]]:
    sizes: dict[str, int] = {}
    origins: dict[str, str] = {}
    tracked: set[str] = set()
    for obj in objects:
        output = subprocess.check_output([nm, "-S", str(obj)], cwd=ROOT)
        for raw_line in output.decode().splitlines():
            parts = raw_line.strip().split()
            if len(parts) < 4:
                continue
            _, size_hex, sym_type, name = parts[:4]
            if name.startswith("ui_ram_audit_entry_"):
                tracked.add(name[len("ui_ram_audit_entry_"):])
            if sym_type.lower() not in {"b", "d", "r"}:
                continue
            try:
                size = int(size_hex, 16)
            except ValueError:
                continue
            sizes[name] = size
            origins.setdefault(name, obj.name)
    return sizes, origins, tracked


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gcc", default="arm-none-eabi-gcc", help="arm-none-eabi gcc path")
    parser.add_argument("--nm", default="arm-none-eabi-nm", help="arm-none-eabi nm path")
    parser.add_argument("sources", nargs="*", default=DEFAULT_SOURCES, help="sources to audit")
    args = parser.parse_args(argv)

    objects = compile_objects(args.gcc, args.sources)
    sizes, origins, tracked = collect_symbols(args.nm, objects)

    if not tracked:
        print("No UI_RAM_AUDIT entries found", file=sys.stderr)
        return 1

    header = "| Symbole | Taille (octets) | Fichier |"
    separator = "| --- | ---: | --- |"
    rows = [header, separator]
    for name in sorted(tracked, key=lambda s: sizes.get(s, 0), reverse=True):
        size = sizes.get(name)
        if size is None:
            continue
        origin = origins.get(name, "?")
        rows.append(f"| {name} | {size} | {origin} |")

    print("\n".join(rows))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
