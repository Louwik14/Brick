# List of all the board related files.
BOARDSRC = \
$(BOARD_PATH)/board.c \
$(BOARD_PATH)/board_flash.c

# Required include directories
BOARDINC = $(BOARD_PATH)

# Shared variables
ALLCSRC += $(BOARDSRC)
ALLINC  += $(BOARDINC)
