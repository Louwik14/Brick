#!/usr/bin/env python3
"""Validate CCRAM (.ram4) layout for Brick firmware builds."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

SECTION_RE = re.compile(
    r"^\s*(?P<idx>\d+)\s+(?P<name>\S+)\s+"
    r"(?P<size>[0-9a-fA-F]+)\s+(?P<vma>[0-9a-fA-F]+)\s+"
    r"(?P<lma>[0-9a-fA-F]+)\s+(?P<fo>[0-9a-fA-F]+)\s+2\*\*\d+"
)

RAM4_BASE = 0x10000000
RAM4_SIZE = 0x10000


def run_objdump(objdump: str, elf: Path) -> list[str]:
    try:
        result = subprocess.check_output([objdump, "-h", str(elf)], text=True)
    except (OSError, subprocess.CalledProcessError) as exc:
        raise SystemExit(f"error: unable to run {objdump!r}: {exc}") from exc
    return result.splitlines()


def parse_sections(lines: list[str]) -> dict[str, dict[str, object]]:
    sections: dict[str, dict[str, object]] = {}
    i = 0
    while i < len(lines):
        line = lines[i]
        match = SECTION_RE.match(line)
        if match:
            name = match.group("name")
            size = int(match.group("size"), 16)
            vma = int(match.group("vma"), 16)
            lma = int(match.group("lma"), 16)
            flags_line = lines[i + 1].strip() if i + 1 < len(lines) else ""
            sections[name] = {
                "size": size,
                "vma": vma,
                "lma": lma,
                "flags": flags_line.split(),
            }
        i += 1
    return sections


def check_ccmram(sections: dict[str, dict[str, object]]) -> int:
    ram4 = sections.get(".ram4")
    if ram4 is None:
        print("error: section .ram4 not found in ELF headers", file=sys.stderr)
        return 1

    size = int(ram4["size"])  # type: ignore[arg-type]
    vma = int(ram4["vma"])  # type: ignore[arg-type]
    flags = {str(flag) for flag in ram4["flags"]}  # type: ignore[arg-type]

    errors = False
    if vma != RAM4_BASE:
        print(
            f"error: .ram4 VMA expected 0x{RAM4_BASE:08X}, got 0x{vma:08X}",
            file=sys.stderr,
        )
        errors = True

    if size > RAM4_SIZE:
        print(
            f"error: .ram4 size {size} exceeds 64 KiB budget", file=sys.stderr
        )
        errors = True

    if "LOAD" in flags or "CONTENTS" in flags:
        print(
            "error: .ram4 should be NOLOAD (no LOAD/CONTENTS flags)",
            file=sys.stderr,
        )
        errors = True

    ram4_init = sections.get(".ram4_init")
    if ram4_init:
        init_size = int(ram4_init["size"])  # type: ignore[arg-type]
        init_flags = {str(flag) for flag in ram4_init["flags"]}  # type: ignore[arg-type]
        if init_size != 0:
            print("error: .ram4_init should be empty", file=sys.stderr)
            errors = True
        if init_size > 0 and ("LOAD" in init_flags or "CONTENTS" in init_flags):
            print(
                "error: .ram4_init must not request loadable data",
                file=sys.stderr,
            )
            errors = True

    if errors:
        return 1

    print(
        "[ccm-audit] .ram4 OK — size=%d bytes, flags=%s"
        % (size, " ".join(sorted(flags)) or "<none>"),
        file=sys.stderr,
    )
    return 0


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", nargs="?", default="build/ch.elf", help="ELF to audit")
    parser.add_argument(
        "--objdump",
        default="arm-none-eabi-objdump",
        help="objdump executable (default: %(default)s)",
    )
    args = parser.parse_args(argv)

    elf_path = Path(args.elf)
    if not elf_path.exists():
        print(f"error: ELF file {elf_path} not found", file=sys.stderr)
        return 1

    lines = run_objdump(args.objdump, elf_path)
    sections = parse_sections(lines)
    return check_ccmram(sections)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))