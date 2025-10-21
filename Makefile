##############################################################################
# Build global options
# NOTE: Can be overridden externally.
#

# Compiler options here.
ifeq ($(USE_OPT),)
  USE_OPT = -O2 -ggdb -fomit-frame-pointer -falign-functions=16
endif

# C specific options here (added to USE_OPT).
ifeq ($(USE_COPT),)
  USE_COPT =
endif

# Optional warnings control (default: enabled).
ifeq ($(USE_WARNINGS),)
  USE_WARNINGS = yes
endif

ifeq ($(USE_WARNINGS),yes)
  WARNINGS_FLAGS = -Wall -Wextra
else
  WARNINGS_FLAGS =
endif

USE_COPT += $(WARNINGS_FLAGS)
# P1/MP5 — handles flipped ON globally for apps/** (deprecated APIs now error).
CFLAGS += -DSEQ_USE_HANDLES=1 -Werror=deprecated-declarations

# C++ specific options here (added to USE_OPT).
ifeq ($(USE_CPPOPT),)
  USE_CPPOPT = -fno-rtti
endif

# Enable this if you want the linker to remove unused code and data.
ifeq ($(USE_LINK_GC),)
  USE_LINK_GC = yes
endif

# Linker extra options here.
ifeq ($(USE_LDOPT),)
  USE_LDOPT = 
endif

# Enable this if you want link time optimizations (LTO).
ifeq ($(USE_LTO),)
  USE_LTO = no
endif

# Enable this if you want to see the full log while compiling.
ifeq ($(USE_VERBOSE_COMPILE),)
  USE_VERBOSE_COMPILE = no
endif

# If enabled, this option makes the build process faster by not compiling
# modules not used in the current configuration.
ifeq ($(USE_SMART_BUILD),)
  USE_SMART_BUILD = yes
endif

#
# Build global options
##############################################################################

##############################################################################
# Architecture or project specific options
#

# Stack size to be allocated to the Cortex-M process stack. This stack is
# the stack used by the main() thread.
ifeq ($(USE_PROCESS_STACKSIZE),)
  USE_PROCESS_STACKSIZE = 0x400
endif

# Stack size to the allocated to the Cortex-M main/exceptions stack. This
# stack is used for processing interrupts and exceptions.
ifeq ($(USE_EXCEPTIONS_STACKSIZE),)
  USE_EXCEPTIONS_STACKSIZE = 0x400
endif

# Enables the use of FPU (no, softfp, hard).
ifeq ($(USE_FPU),)
  USE_FPU = no
endif

# FPU-related options.
ifeq ($(USE_FPU_OPT),)
  USE_FPU_OPT = -mfloat-abi=$(USE_FPU) -mfpu=fpv4-sp-d16
endif

#
# Architecture or project specific options
##############################################################################

##############################################################################
# Project, target, sources and paths
#

# Define project name here
PROJECT = ch

# Target settings.
MCU  = cortex-m4

# Imported source files and paths.
CHIBIOS  := ./chibios2111
CONFDIR  := ./cfg
BUILDDIR := ./build
DEPDIR   := ./.dep

# Licensing files.
ifeq ($(filter lint-cppcheck check-host,$(MAKECMDGOALS)),)
include $(CHIBIOS)/os/license/license.mk
# Startup files.
include $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/mk/startup_stm32f4xx.mk
# HAL-OSAL files (optional).
include $(CHIBIOS)/os/hal/hal.mk
include $(CHIBIOS)/os/hal/ports/STM32/STM32F4xx/platform.mk

# --- Board: utiliser le dossier local du projet ---
PROJECT_DIR := .
BOARD_PATH  := $(PROJECT_DIR)/board
include $(BOARD_PATH)/board.mk

include $(CHIBIOS)/os/hal/osal/rt-nil/osal.mk
# RTOS files (optional).
include $(CHIBIOS)/os/rt/rt.mk
include $(CHIBIOS)/os/common/ports/ARMv7-M/compilers/GCC/mk/port.mk
# Auto-build files in ./source recursively.
-include $(CHIBIOS)/tools/mk/autobuild.mk
# Other files (optional).
-include $(CHIBIOS)/os/test/test.mk
-include $(CHIBIOS)/test/rt/rt_test.mk
-include $(CHIBIOS)/test/oslib/oslib_test.mk
endif

# Define linker script file here
LDSCRIPT= board/STM32F429xI.ld

# C sources that can be compiled in ARM or THUMB mode depending on the global
# setting.
CSRC = $(ALLCSRC) \
       main.c \
       $(CHIBIOS)/os/hal/lib/streams/chprintf.c \
       $(CHIBIOS)/os/hal/lib/streams/memstreams.c \
       $(CHIBIOS)/os/various/syscalls.c \
       $(wildcard usb/*.c) \
       $(wildcard midi/*.c) \
       $(wildcard drivers/*.c) \
       $(wildcard ui/*.c) \
       $(wildcard apps/*.c) \
       $(wildcard cart/*.c) \
       $(wildcard core/*.c) \
       $(wildcard core/arp/*.c) \
       $(wildcard core/seq/*.c) \
       core/seq/runtime/seq_runtime_cold.c \
       core/seq/runtime/seq_runtime_layout.c \
       $(wildcard core/seq/reader/*.c) \
       
       
       
       
       
     

       

# C++ sources that can be compiled in ARM or THUMB mode depending on the global
# setting.
CPPSRC = $(ALLCPPSRC)

# List ASM source files here.
ASMSRC = $(ALLASMSRC)

# List ASM with preprocessor source files here.
ASMXSRC = $(ALLXASMSRC)

# Inclusion directories.
INCDIR = $(CONFDIR) $(ALLINC) $(CHIBIOS)/os/hal/lib/streams


# Define C warning options here.
CWARN = -Wall -Wextra -Wundef -Wstrict-prototypes

# Define C++ warning options here.
CPPWARN = -Wall -Wextra -Wundef

#
# Project, target, sources and paths
##############################################################################

##############################################################################
# Start of user section
#

# List all user C define here, like -D_DEBUG=1
UDEFS =

# Define ASM defines here
UADEFS =

# List all user directories here
UINCDIR = usb midi drivers ui apps cart cfg core core/spec core/seq core/arp

# List the user directory to look for the libraries here
ULIBDIR =

# List all user libraries here
ULIBS = -lm

USE_CCM = yes
#
# End of user section
##############################################################################

##############################################################################
# Common rules
#
LDFLAGS += -Wl,-Map=$(BUILD)/ch.map,--print-memory-usage

RULESPATH = board

ifeq ($(filter lint-cppcheck check-host,$(MAKECMDGOALS)),)
include $(RULESPATH)/arm-none-eabi.mk
include $(RULESPATH)/rules.mk
endif

CPPCHECK ?= cppcheck
CPPCHECK_FLAGS ?= --enable=warning,style,performance --std=c11

.PHONY: lint-cppcheck
lint-cppcheck:
	@echo "Running cppcheck on core/ and ui/"
	$(CPPCHECK) $(CPPCHECK_FLAGS) core ui

#
# Common rules
##############################################################################

##############################################################################
# Custom rules
#

#
# Custom rules
##############################################################################

.PHONY: check_no_legacy_includes_apps

check_no_legacy_includes_apps:
	@if grep -R -nE '#include\s+"seq_(project|model)\.h"' apps > /dev/null; then \
	  echo "Forbidden legacy include in apps/** detected"; \
	  grep -R -nE '#include\s+"seq_(project|model)\.h"' apps; \
	  exit 1; \
	fi

POST_MAKE_ALL_RULE_HOOK += check_no_legacy_includes_apps

HOST_CC ?= gcc
HOST_CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -g \
               -DUI_DEBUG_TRACE_MODE_TRANSITION \
               -DUI_DEBUG_TRACE_LED_BACKEND \
               -DUI_LED_BACKEND_TESTING
HOST_TEST_DIR := $(BUILDDIR)/host
HOST_SEQ_MODEL_TEST := $(HOST_TEST_DIR)/seq_model_tests
HOST_SEQ_HOLD_TEST  := $(HOST_TEST_DIR)/seq_hold_runtime_tests
HOST_UI_MODE_TEST   := $(HOST_TEST_DIR)/ui_mode_transition_tests
HOST_UI_EDGE_TEST   := $(HOST_TEST_DIR)/ui_mode_edgecase_tests
HOST_UI_TRACK_PMUTE_TEST := $(HOST_TEST_DIR)/ui_track_pmute_regression_tests
HOST_SEQ_TRACK_CODEC_TEST := $(HOST_TEST_DIR)/seq_track_codec_tests
HOST_SEQ_READER_TEST := $(HOST_TEST_DIR)/seq_reader_tests
HOST_SEQ_RUNTIME_LAYOUT_TEST := $(HOST_TEST_DIR)/seq_runtime_layout_tests
HOST_SEQ_RUNTIME_COLD_TEST := $(HOST_TEST_DIR)/seq_runtime_cold_project_tests
HOST_SEQ_RUNTIME_CART_META_TEST := $(HOST_TEST_DIR)/seq_runtime_cold_cart_meta_tests

ifeq ($(OS),Windows_NT)
HOST_CC_AVAILABLE := $(strip $(shell where $(HOST_CC) >NUL 2>NUL && echo yes))
HOST_CC_HINT := Installez "MSYS2 / mingw-w64" et exposez gcc (ou définissez HOST_CC=clang).
else
HOST_CC_AVAILABLE := $(strip $(shell command -v $(HOST_CC) >/dev/null 2>&1 && echo yes))
HOST_CC_HINT := Installez gcc (ex: `sudo apt install build-essential`) ou définissez HOST_CC=clang.
endif

.PHONY: check-host
ifeq ($(HOST_CC_AVAILABLE),yes)
check-host: $(HOST_SEQ_MODEL_TEST) $(HOST_SEQ_HOLD_TEST) $(HOST_UI_MODE_TEST) $(HOST_UI_EDGE_TEST) $(HOST_UI_TRACK_PMUTE_TEST) $(HOST_SEQ_TRACK_CODEC_TEST) $(HOST_SEQ_READER_TEST) $(HOST_SEQ_RUNTIME_LAYOUT_TEST) $(HOST_SEQ_RUNTIME_COLD_TEST) $(HOST_SEQ_RUNTIME_CART_META_TEST)
	@echo "Running host sequencer model tests"
	$(HOST_SEQ_MODEL_TEST)
	@echo "Running host hold/runtime bridge tests"
	$(HOST_SEQ_HOLD_TEST)
	@echo "Running host UI mode tests"
	$(HOST_UI_MODE_TEST)
	@echo "Running host UI edge-case tests"
	$(HOST_UI_EDGE_TEST)
	@echo "Running host UI track/pmute regression tests"
	$(HOST_UI_TRACK_PMUTE_TEST)
	@echo "Running track codec regression tests"
	$(HOST_SEQ_TRACK_CODEC_TEST)
	@echo "Running reader facade tests"
	$(HOST_SEQ_READER_TEST)
	@echo "Running runtime layout tests"
	$(HOST_SEQ_RUNTIME_LAYOUT_TEST)
	@echo "Running runtime cold view tests"
	$(HOST_SEQ_RUNTIME_COLD_TEST)
	@echo "Running runtime cart metadata view tests"
	$(HOST_SEQ_RUNTIME_CART_META_TEST)
else
check-host:
	@echo "error: host compiler '$(HOST_CC)' introuvable pour make check-host."
	@echo "hint: $(HOST_CC_HINT)"
	@echo "hint: make check-host HOST_CC=<compilateur>"
	@exit 1
endif

$(HOST_SEQ_MODEL_TEST): tests/seq_model_tests.c core/seq/seq_model.c core/seq/seq_model_consts.c
	@mkdir -p $(HOST_TEST_DIR)
	$(HOST_CC) $(HOST_CFLAGS) -I. $^ -o $@

$(HOST_SEQ_HOLD_TEST): tests/seq_hold_runtime_tests.c apps/seq_led_bridge.c apps/seq_recorder.c core/seq/seq_model.c core/seq/seq_model_consts.c core/seq/seq_live_capture.c core/seq/seq_project.c core/seq/seq_runtime.c core/seq/runtime/seq_runtime_layout.c core/seq/runtime/seq_runtime_cold.c tests/stubs/seq_engine_runner_stub.c tests/stubs/ui_led_backend_stub.c apps/ui_keyboard_app.c apps/kbd_chords_dict.c board/board_flash.c cart/cart_registry.c
	@mkdir -p $(HOST_TEST_DIR)
	$(HOST_CC) $(HOST_CFLAGS) -Itests/stubs -Iui -Iapps -Imidi -Icore -Icart -Iboard -I. \
	tests/seq_hold_runtime_tests.c apps/seq_led_bridge.c apps/seq_recorder.c core/seq/seq_model.c core/seq/seq_model_consts.c core/seq/seq_live_capture.c core/seq/seq_project.c core/seq/seq_runtime.c core/seq/runtime/seq_runtime_layout.c core/seq/runtime/seq_runtime_cold.c tests/stubs/seq_engine_runner_stub.c tests/stubs/ui_led_backend_stub.c \
	apps/ui_keyboard_app.c apps/kbd_chords_dict.c board/board_flash.c cart/cart_registry.c -o $@

$(HOST_UI_MODE_TEST): tests/ui_mode_transition_tests.c ui/ui_shortcuts.c apps/seq_led_bridge.c apps/seq_recorder.c core/seq/seq_model.c core/seq/seq_model_consts.c core/seq/seq_live_capture.c core/seq/seq_project.c core/seq/seq_runtime.c core/seq/runtime/seq_runtime_layout.c core/seq/runtime/seq_runtime_cold.c tests/stubs/seq_engine_runner_stub.c tests/stubs/ui_led_seq_stub.c tests/stubs/ui_mute_backend_stub.c apps/ui_keyboard_app.c apps/kbd_chords_dict.c board/board_flash.c cart/cart_registry.c
	@mkdir -p $(HOST_TEST_DIR)
	$(HOST_CC) $(HOST_CFLAGS) -Itests/stubs -Iui -Iapps -Imidi -Icore -Icart -Iboard -I. \
	tests/ui_mode_transition_tests.c ui/ui_shortcuts.c apps/seq_led_bridge.c apps/seq_recorder.c core/seq/seq_model.c core/seq/seq_model_consts.c core/seq/seq_live_capture.c core/seq/seq_project.c core/seq/seq_runtime.c core/seq/runtime/seq_runtime_layout.c core/seq/runtime/seq_runtime_cold.c tests/stubs/seq_engine_runner_stub.c tests/stubs/ui_led_seq_stub.c tests/stubs/ui_mute_backend_stub.c \
	apps/ui_keyboard_app.c apps/kbd_chords_dict.c board/board_flash.c cart/cart_registry.c -o $@

$(HOST_UI_EDGE_TEST): tests/ui_mode_edgecase_tests.c ui/ui_mode_transition.c ui/ui_shortcuts.c \
	        tests/stubs/ui_mute_backend_stub.c tests/stubs/ui_model_stub.c core/seq/runtime/seq_runtime_layout.c core/seq/runtime/seq_runtime_cold.c core/seq/seq_runtime.c core/seq/seq_project.c core/seq/seq_model.c core/seq/seq_model_consts.c cart/cart_registry.c board/board_flash.c
	@mkdir -p $(HOST_TEST_DIR)
	$(HOST_CC) $(HOST_CFLAGS) -Itests/stubs -Iui -Iapps -Imidi -Icore -Icart -Iboard -I. \
	tests/ui_mode_edgecase_tests.c ui/ui_mode_transition.c ui/ui_shortcuts.c \
	        tests/stubs/ui_mute_backend_stub.c tests/stubs/ui_model_stub.c core/seq/runtime/seq_runtime_layout.c core/seq/runtime/seq_runtime_cold.c core/seq/seq_runtime.c core/seq/seq_project.c core/seq/seq_model.c core/seq/seq_model_consts.c cart/cart_registry.c board/board_flash.c -o $@

$(HOST_UI_TRACK_PMUTE_TEST): tests/ui_track_pmute_regression_tests.c ui/ui_backend.c ui/ui_shortcuts.c \
	                ui/ui_mode_transition.c ui/ui_mute_backend.c ui/ui_led_backend.c ui/ui_led_seq.c ui/ui_led_layout.c \
	                apps/seq_led_bridge.c core/seq/seq_model.c core/seq/seq_model_consts.c core/seq/seq_live_capture.c core/seq/seq_project.c core/seq/seq_runtime.c core/seq/runtime/seq_runtime_layout.c core/seq/runtime/seq_runtime_cold.c \
	                tests/stubs/seq_engine_runner_stub.c tests/stubs/ui_backend_test_stubs.c \
	                tests/stubs/drv_leds_addr_stub.c tests/stubs/ui_overlay_stub.c tests/stubs/ui_model_stub.c \
	                tests/stubs/board_flash_stub.c
	@mkdir -p $(HOST_TEST_DIR)
	$(HOST_CC) $(HOST_CFLAGS) -Itests/stubs -Iui -Iapps -Imidi -Icore -Icart -Iboard -Idrivers -I. \
	tests/ui_track_pmute_regression_tests.c ui/ui_backend.c ui/ui_shortcuts.c \
	ui/ui_mode_transition.c ui/ui_mute_backend.c ui/ui_led_backend.c ui/ui_led_seq.c ui/ui_led_layout.c \
	apps/seq_led_bridge.c core/seq/seq_model.c core/seq/seq_model_consts.c core/seq/seq_live_capture.c core/seq/seq_project.c core/seq/seq_runtime.c core/seq/runtime/seq_runtime_layout.c core/seq/runtime/seq_runtime_cold.c \
	tests/stubs/seq_engine_runner_stub.c tests/stubs/ui_backend_test_stubs.c \
	tests/stubs/drv_leds_addr_stub.c tests/stubs/ui_overlay_stub.c tests/stubs/ui_model_stub.c \
	tests/stubs/board_flash_stub.c -o $@
$(HOST_SEQ_TRACK_CODEC_TEST): tests/seq_track_codec_tests.c core/seq/seq_model.c core/seq/seq_model_consts.c core/seq/seq_project.c core/seq/runtime/seq_runtime_layout.c core/seq/runtime/seq_runtime_cold.c core/seq/seq_runtime.c
	@mkdir -p $(HOST_TEST_DIR)
	$(HOST_CC) $(HOST_CFLAGS) -DBRICK_EXPERIMENTAL_PATTERN_CODEC_V2=1 -I. -Icore -Icart -Iboard \
	tests/seq_track_codec_tests.c core/seq/seq_model.c core/seq/seq_model_consts.c core/seq/seq_project.c core/seq/runtime/seq_runtime_layout.c core/seq/runtime/seq_runtime_cold.c core/seq/seq_runtime.c -o $@

$(HOST_SEQ_READER_TEST): tests/seq_reader_tests.c core/seq/reader/seq_reader.c core/seq/seq_model.c core/seq/seq_model_consts.c core/seq/seq_project.c core/seq/seq_runtime.c core/seq/runtime/seq_runtime_layout.c core/seq/runtime/seq_runtime_cold.c
	@mkdir -p $(HOST_TEST_DIR)
	$(HOST_CC) $(HOST_CFLAGS) -I. -Icore -Icart -Iboard \
        tests/seq_reader_tests.c core/seq/reader/seq_reader.c core/seq/seq_model.c core/seq/seq_model_consts.c core/seq/seq_project.c core/seq/seq_runtime.c core/seq/runtime/seq_runtime_layout.c core/seq/runtime/seq_runtime_cold.c -o $@

$(HOST_SEQ_RUNTIME_LAYOUT_TEST): tests/seq_runtime_layout_tests.c core/seq/runtime/seq_runtime_layout.c core/seq/runtime/seq_runtime_cold.c core/seq/seq_runtime.c core/seq/seq_project.c core/seq/seq_model.c core/seq/seq_model_consts.c cart/cart_registry.c board/board_flash.c
	@mkdir -p $(HOST_TEST_DIR)
	$(HOST_CC) $(HOST_CFLAGS) -I. -Icore -Icart -Iboard -Iui \
	tests/seq_runtime_layout_tests.c core/seq/runtime/seq_runtime_layout.c core/seq/runtime/seq_runtime_cold.c core/seq/seq_runtime.c core/seq/seq_project.c core/seq/seq_model.c core/seq/seq_model_consts.c cart/cart_registry.c board/board_flash.c -o $@

$(HOST_SEQ_RUNTIME_COLD_TEST): tests/seq_runtime_cold_project_tests.c core/seq/runtime/seq_runtime_cold.c core/seq/runtime/seq_runtime_layout.c core/seq/seq_runtime.c core/seq/seq_project.c core/seq/seq_model.c core/seq/seq_model_consts.c cart/cart_registry.c board/board_flash.c
	@mkdir -p $(HOST_TEST_DIR)
	$(HOST_CC) $(HOST_CFLAGS) -I. -Icore -Icart -Iboard \
	tests/seq_runtime_cold_project_tests.c core/seq/runtime/seq_runtime_cold.c core/seq/runtime/seq_runtime_layout.c core/seq/seq_runtime.c core/seq/seq_project.c core/seq/seq_model.c core/seq/seq_model_consts.c cart/cart_registry.c board/board_flash.c -o $@

$(HOST_SEQ_RUNTIME_CART_META_TEST): tests/seq_runtime_cold_cart_meta_tests.c core/seq/runtime/seq_runtime_cold.c core/seq/runtime/seq_runtime_layout.c core/seq/seq_runtime.c core/seq/seq_project.c core/seq/seq_model.c core/seq/seq_model_consts.c cart/cart_registry.c board/board_flash.c
	@mkdir -p $(HOST_TEST_DIR)
	$(HOST_CC) $(HOST_CFLAGS) -I. -Icore -Icart -Iboard \
	tests/seq_runtime_cold_cart_meta_tests.c core/seq/runtime/seq_runtime_cold.c core/seq/runtime/seq_runtime_layout.c core/seq/seq_runtime.c core/seq/seq_project.c core/seq/seq_model.c core/seq/seq_model_consts.c cart/cart_registry.c board/board_flash.c -o $@
