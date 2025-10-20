#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 <elf>" >&2
  exit 1
fi

elf=$1

if [[ ! -f $elf ]]; then
  echo "error: ELF '$elf' not found" >&2
  exit 1
fi

for tool in arm-none-eabi-objdump arm-none-eabi-nm; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "error: required tool '$tool' not found in PATH" >&2
    exit 1
  fi
done

mapfile -t section_lines < <(arm-none-eabi-objdump -h "$elf" | awk '$2 == ".ccmram" {print; getline; print}')
if ((${#section_lines[@]} == 0)); then
  echo "error: .ccmram section missing" >&2
  exit 1
fi

info_line=${section_lines[0]}
flags_line=""
if ((${#section_lines[@]} > 1)); then
  flags_line=${section_lines[1]}
fi

vma=$(awk '{print $4}' <<<"$info_line")
size=$(awk '{print $3}' <<<"$info_line")

if [[ ${vma,,} != 10000000 ]]; then
  echo "error: .ccmram VMA is 0x$vma (expected 0x10000000)" >&2
  exit 1
fi

if [[ -n $flags_line ]]; then
  if grep -qi 'LOAD' <<<"$flags_line"; then
    echo "error: .ccmram section has LOAD flag (should be NOLOAD)" >&2
    exit 1
  fi
  if grep -qi 'CONTENTS' <<<"$flags_line"; then
    echo "error: .ccmram section has CONTENTS flag (should be NOLOAD)" >&2
    exit 1
  fi
fi

ccm_start=$((0x10000000))
ccm_end=$((ccm_start + 0x10000))

while read -r addr size_hex type name _; do
  [[ -z $addr ]] && continue
  if [[ ! $addr =~ ^[0-9a-fA-F]+$ ]]; then
    continue
  fi
  addr_dec=$((16#$addr))
  if (( addr_dec < ccm_start || addr_dec >= ccm_end )); then
    continue
  fi

  lower_name=${name,,}
  if [[ $lower_name == *dma* || $lower_name == *_rx* || $lower_name == *_tx* ]]; then
    echo "error: symbol '$name' (type $type) must not live in .ccmram" >&2
    exit 1
  fi
  if [[ $type =~ [Rr] ]]; then
    echo "error: read-only symbol '$name' detected in .ccmram" >&2
    exit 1
  fi
  # Accept only zero-initialized data (B/b) or untyped linker symbols.
  if [[ ! $type =~ ^[BbWw]$ && $name != __ccmram_start__ && $name != __ccmram_end__ ]]; then
    echo "error: unexpected symbol '$name' (type $type) in .ccmram" >&2
    exit 1
  fi
done < <(arm-none-eabi-nm --print-size --numeric-sort "$elf")

echo "ccmram ok: size=0x$size vma=0x$vma"
