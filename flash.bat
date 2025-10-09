@echo off
REM === Script de compilation + flash ===

REM Ajouter la toolchain ARM au PATH local
set PATH=C:\ChibiStudio\tools\GNU Tools ARM Embedded\11.3 2022.08\bin;%PATH%

REM Se placer dans le dossier du projet
cd /d "%~dp0"

REM Compilation incrémentale (pas de clean)
echo.
echo === Compilation du projet ===
make -j8 all
if errorlevel 1 (
    echo !!!
    echo !!! Erreur de compilation !!!
    echo !!!
    pause
    exit /b
)

REM Flash avec OpenOCD
echo.
echo === Flash du microcontrôleur ===
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg -c "program build/ch.elf verify reset exit"

pause
