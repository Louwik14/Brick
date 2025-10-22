#!/usr/bin/env bash
set -euo pipefail
if command -v apt-get >/dev/null 2>&1; then
  sudo apt-get update
  sudo apt-get install -y gcc-arm-none-eabi binutils-arm-none-eabi
fi
which arm-none-eabi-gcc || true
