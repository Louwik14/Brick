#!/usr/bin/env bash
set -euo pipefail
pacman -Sy --noconfirm
pacman -S --needed --noconfirm \
  mingw-w64-ucrt-x86_64-arm-none-eabi-gcc \
  mingw-w64-ucrt-x86_64-arm-none-eabi-binutils \
  mingw-w64-ucrt-x86_64-arm-none-eabi-newlib
which arm-none-eabi-gcc
arm-none-eabi-gcc --version
