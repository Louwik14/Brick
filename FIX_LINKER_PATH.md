# Fix linker script resolution on Windows

## Root cause

When the project is built from ChibiStudio's MSYS environment, GNU Make
produces POSIX-style absolute paths such as `/cygdrive/c/...` for
`STARTUPLD` and the custom linker script. These paths are passed verbatim to
`arm-none-eabi-ld`, which is a native Windows executable and therefore does
not understand the `/cygdrive` prefix. As a result, the linker reports that
`STM32F429xI.ld` cannot be opened even though the file exists.

## Implemented changes

- Added a small path-normalisation helper to `board/rules.mk` that translates
  `/cygdrive/x/...` and `/x/...` MSYS/Cygwin paths into `x:/...` before they
  are supplied to the linker.
- Updated the linker invocation so both `--library-path=` and `--script=` use
  the normalised paths when running on Windows hosts.

## How to verify

1. Clean any previous build artefacts: `make clean`.
2. Build the project from ChibiStudio/MSYS: `make`. The linker now resolves
   `STM32F429xI.ld` correctly and the build completes.
3. (Optional) On non-Windows systems, run `make` to confirm the changes are
   transparent; the helper returns the original POSIX paths when normalisation
   is not required.
