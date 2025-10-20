@echo off
REM ===============================================================
REM  Brick5 Memory Audit Script (Windows .bat version)
REM  Generates updated audit files from the latest build/ch.elf
REM  Output folder: tools\audit\
REM ===============================================================

cd /d "%~dp0\.."    REM ← IMPORTANT : revient à la racine du projet

setlocal enabledelayedexpansion
set ELF=build\ch.elf
set OUTDIR=tools\audit

echo ===============================================================
echo   STM32F429 + ChibiOS Memory Audit Utility
echo   Using ELF: %ELF%
echo   Output dir: %OUTDIR%
echo ===============================================================

REM --- Check ELF existence ---
if not exist "%ELF%" (
    echo [ERROR] File %ELF% not found. Run "make" first.
    pause
    exit /b 1
)

REM --- Create audit directory if missing ---
if not exist "%OUTDIR%" (
    mkdir "%OUTDIR%"
)

REM --- 1. Section summary (.text, .bss, .data, .ram4, etc.) ---
echo [1/9] Creating audit_sections.txt
arm-none-eabi-size -A "%ELF%" > "%OUTDIR%\audit_sections.txt"

REM --- 2. Full .bss symbols ---
echo [2/9] Creating nm_bss_all.txt
arm-none-eabi-nm --print-size --size-sort --radix=d "%ELF%" | findstr "\.bss" > "%OUTDIR%\nm_bss_all.txt"

REM --- 3. Full .data symbols ---
echo [3/9] Creating nm_data_all.txt
arm-none-eabi-nm --print-size --size-sort --radix=d "%ELF%" | findstr "\.data" > "%OUTDIR%\nm_data_all.txt"

REM --- 4. Global RAM Top (200 largest symbols) ---
echo [4/9] Creating audit_ram_top.txt
arm-none-eabi-nm --print-size --size-sort --radix=d "%ELF%" > "%OUTDIR%\temp_all.txt"
powershell -NoProfile -Command ^
  "Get-Content '%OUTDIR%\temp_all.txt' | Sort-Object {[int]($_ -split ' +')[1]} -Descending | Select-Object -First 200 | Out-File '%OUTDIR%\audit_ram_top.txt'"
del "%OUTDIR%\temp_all.txt"

REM --- 5. Top 50 .bss symbols ---
echo [5/9] Creating audit_bss_top.txt
powershell -NoProfile -Command ^
  "Get-Content '%OUTDIR%\nm_bss_all.txt' | Sort-Object {[int]($_ -split ' +')[1]} -Descending | Select-Object -First 50 | Out-File '%OUTDIR%\audit_bss_top.txt'"

REM --- 6. Top 50 .data symbols ---
echo [6/9] Creating audit_data_top.txt
powershell -NoProfile -Command ^
  "Get-Content '%OUTDIR%\nm_data_all.txt' | Sort-Object {[int]($_ -split ' +')[1]} -Descending | Select-Object -First 50 | Out-File '%OUTDIR%\audit_data_top.txt'"

REM --- 7. Symbols mapped in .ram4 section ---
echo [7/9] Creating audit_ram4_symbols.txt
arm-none-eabi-nm --print-size --size-sort --numeric-sort "%ELF%" | findstr "ram4" > "%OUTDIR%\audit_ram4_symbols.txt"

REM --- 8. Mapping snapshot of .ram4 from .map file ---
echo [8/9] Creating audit_map_ram4.txt
if exist build\ch.map (
    findstr /C:".ram4" build\ch.map > "%OUTDIR%\audit_map_ram4.txt"
) else (
    echo [WARN] build\ch.map not found, skipping. > "%OUTDIR%\audit_map_ram4.txt"
)

REM --- 9. Placeholder for header audit (optional) ---
echo [9/9] Creating audit_headers.txt (placeholder)
echo "Header audit not automated yet." > "%OUTDIR%\audit_headers.txt"

echo.
echo ===============================================================
echo   ✅ Memory Audit Completed Successfully
echo   Files generated in: %OUTDIR%
echo ===============================================================

pause
endlocal
