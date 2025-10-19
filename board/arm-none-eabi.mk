##############################################################################
# Compiler settings
#

TRGT = arm-none-eabi-
# --- Override optionnel: forcer un répertoire toolchain explicite (gère les espaces) ---
# Exemple d’usage dans le Makefile projet :
#   ARMGCC_DIR := C:/ChibiStudio/tools/gnu tools arm embedded/11.3 2022.08
ifdef ARMGCC_DIR
ARMGCC_BIN := $(ARMGCC_DIR)/bin
CC   := "$(ARMGCC_BIN)/arm-none-eabi-gcc"
CPPC := "$(ARMGCC_BIN)/arm-none-eabi-g++"
# Enable loading with g++ only if you need C++ runtime support.
LD   := "$(ARMGCC_BIN)/arm-none-eabi-gcc"
CP   := "$(ARMGCC_BIN)/arm-none-eabi-objcopy"
AS   := "$(ARMGCC_BIN)/arm-none-eabi-gcc" -x assembler-with-cpp
AR   := "$(ARMGCC_BIN)/arm-none-eabi-ar"
OD   := "$(ARMGCC_BIN)/arm-none-eabi-objdump"
SZ   := "$(ARMGCC_BIN)/arm-none-eabi-size"
HEX  := $(CP) -O ihex
BIN  := $(CP) -O binary
endif

CC   = $(TRGT)gcc
CPPC = $(TRGT)g++
# Enable loading with g++ only if you need C++ runtime support.
# NOTE: You can use C++ even without C++ support if you are careful. C++
#       runtime support makes code size explode.
LD   = $(TRGT)gcc
#LD   = $(TRGT)g++
CP   = $(TRGT)objcopy
AS   = $(TRGT)gcc -x assembler-with-cpp
AR   = $(TRGT)ar
OD   = $(TRGT)objdump
SZ   = $(TRGT)size
HEX  = $(CP) -O ihex
BIN  = $(CP) -O binary

#
# Compiler settings
##############################################################################
