# ARM Cortex-Mx common makefile scripts and rules.

##############################################################################
# Processing options coming from the upper Makefile.
#

# Compiler options
OPT    := $(USE_OPT)
COPT   := $(USE_COPT)
CPPOPT := $(USE_CPPOPT)

# Garbage collection
ifeq ($(USE_LINK_GC),yes)
  OPT   += -ffunction-sections -fdata-sections -fno-common
  LDOPT := ,--gc-sections
else
  LDOPT :=
endif

# Linker extra options
ifneq ($(USE_LDOPT),)
  LDOPT := $(LDOPT),$(USE_LDOPT)
endif

# Link time optimizations
ifneq ($(USE_LTO),no)
  ifeq ($(USE_LTO),yes)
    OPT += -flto=auto
  else
    OPT += -flto=$(USE_LTO)
  endif
endif

# FPU options default (Cortex-M4 and Cortex-M7 single precision).
ifeq ($(USE_FPU_OPT),)
  USE_FPU_OPT = -mfloat-abi=$(USE_FPU) -mfpu=fpv4-sp-d16
endif

# FPU-related options
ifeq ($(USE_FPU),)
  USE_FPU = no
endif
ifneq ($(USE_FPU),no)
  OPT    += $(USE_FPU_OPT)
  DDEFS  += -DCORTEX_USE_FPU=TRUE
  DADEFS += -DCORTEX_USE_FPU=TRUE
else
  DDEFS  += -DCORTEX_USE_FPU=FALSE
  DADEFS += -DCORTEX_USE_FPU=FALSE
endif

# Process stack size
ifeq ($(USE_PROCESS_STACKSIZE),)
  LDOPT := $(LDOPT),--defsym=__process_stack_size__=0x400
else
  LDOPT := $(LDOPT),--defsym=__process_stack_size__=$(USE_PROCESS_STACKSIZE)
endif

# Exceptions stack size
ifeq ($(USE_EXCEPTIONS_STACKSIZE),)
  LDOPT := $(LDOPT),--defsym=__main_stack_size__=0x400
else
  LDOPT := $(LDOPT),--defsym=__main_stack_size__=$(USE_EXCEPTIONS_STACKSIZE)
endif

# Output directory and files
ifeq ($(BUILDDIR),)
  BUILDDIR = build
endif
ifeq ($(BUILDDIR),.)
  BUILDDIR = build
endif

# Dependencies directory
ifeq ($(DEPDIR),)
  DEPDIR = .dep
endif
ifeq ($(DEPDIR),.)
  DEPDIR = .dep
endif

OUTFILES := $(BUILDDIR)/$(PROJECT).elf \
            $(BUILDDIR)/$(PROJECT).hex \
            $(BUILDDIR)/$(PROJECT).bin \
            $(BUILDDIR)/$(PROJECT).dmp \
            $(BUILDDIR)/$(PROJECT).list

ifdef SREC
  OUTFILES += $(BUILDDIR)/$(PROJECT).srec
endif

# Source files groups and paths
SRC      := $(CSRC) $(CPPSRC)
SRCPATHS += $(sort $(dir $(ASMXSRC)) $(dir $(ASMSRC)) $(dir $(SRC)))

# Various directories
OBJDIR    := $(BUILDDIR)/obj
LSTDIR    := $(BUILDDIR)/lst

# Object files groups
COBJS    := $(addprefix $(OBJDIR)/, $(notdir $(CSRC:.c=.o)))
CPPOBJS  := $(addprefix $(OBJDIR)/, $(notdir $(patsubst %.cpp, %.o, $(filter %.cpp, $(CPPSRC)))))
CCOBJS   := $(addprefix $(OBJDIR)/, $(notdir $(patsubst %.cc, %.o, $(filter %.cc, $(CPPSRC)))))
ASMOBJS  := $(addprefix $(OBJDIR)/, $(notdir $(ASMSRC:.s=.o)))
ASMXOBJS := $(addprefix $(OBJDIR)/, $(notdir $(ASMXSRC:.S=.o)))
OBJS     += $(ASMXOBJS) $(ASMOBJS) $(COBJS) $(CPPOBJS) $(CCOBJS)

# Paths
IINCDIR   := $(sort $(patsubst %,-I%,$(INCDIR) $(DINCDIR) $(UINCDIR)))
LLIBDIR   := $(sort $(patsubst %,-L%,$(DLIBDIR) $(ULIBDIR)))

# Normalize paths for native Windows toolchains (no cygpath available).
ifeq ($(OS),Windows_NT)
define drive_path
$(strip $(firstword $(subst /, ,$1)):$(if $(filter $(firstword $(subst /, ,$1)),$1),/,/$(patsubst $(firstword $(subst /, ,$1))/%,%,$1))))
endef

define native_path
$(strip \
  $(if $(filter /cygdrive/%,$1),\
    $(call drive_path,$(patsubst /cygdrive/%,%,$1)),\
    $(if $(filter /?/%,$1),\
      $(call drive_path,$(patsubst /%,%,$1)),\
      $1)))
endef

STARTUPLD_NATIVE := $(call native_path,$(STARTUPLD))
LDSCRIPT_NATIVE  := $(call native_path,$(LDSCRIPT))
else
STARTUPLD_NATIVE := $(STARTUPLD)
LDSCRIPT_NATIVE  := $(LDSCRIPT)
endif

# Macros
DEFS      := $(DDEFS) $(UDEFS)
ADEFS     := $(DADEFS) $(UADEFS)

# Libs
LIBS      := $(DLIBS) $(ULIBS)

# Various settings
MCFLAGS   := -mcpu=$(MCU) -mthumb
ODFLAGS   = -x --syms
ASFLAGS   = $(MCFLAGS) $(OPT) -Wa,-amhls=$(LSTDIR)/$(notdir $(<:.s=.lst)) $(ADEFS)
ASXFLAGS  = $(MCFLAGS) $(OPT) -Wa,-amhls=$(LSTDIR)/$(notdir $(<:.S=.lst)) $(ADEFS)
ifeq ($(USE_LTO),no)
  CFLAGS    = $(MCFLAGS) $(OPT) $(COPT) $(CWARN) -Wa,-alms=$(LSTDIR)/$(notdir $(<:.c=.lst)) $(DEFS)
  CPPFLAGS  = $(MCFLAGS) $(OPT) $(CPPOPT) $(CPPWARN) -Wa,-alms=$(LSTDIR)/$(notdir $(<:.cpp=.lst)) $(DEFS)
else
  CFLAGS    = $(MCFLAGS) $(OPT) $(COPT) $(CWARN) $(DEFS)
  CPPFLAGS  = $(MCFLAGS) $(OPT) $(CPPOPT) $(CPPWARN) $(DEFS)
endif

# --- Windows path normalization (kill /cygdrive/ prefixes) ---
# Convertit /cygdrive/c/... ou /c/... en c:/...
LDS_WIN := $(LDSCRIPT)
STARTUPLD_WIN := $(STARTUPLD)

ifeq ($(OS),Windows_NT)
  # /cygdrive/c/... -> c:/...
  LDS_WIN := $(patsubst /cygdrive/c/%,c:/%,$(LDS_WIN))
  STARTUPLD_WIN := $(patsubst /cygdrive/c/%,c:/%,$(STARTUPLD_WIN))
  # /c/... -> c:/...
  LDS_WIN := $(patsubst /c/%,c:/%,$(LDS_WIN))
  STARTUPLD_WIN := $(patsubst /c/%,c:/%,$(STARTUPLD_WIN))
endif

# Debug (temporaire) :
# $(info LDSCRIPT=$(LDSCRIPT))
# $(info LDS_WIN=$(LDS_WIN))
# $(info STARTUPLD=$(STARTUPLD))
# $(info STARTUPLD_WIN=$(STARTUPLD_WIN))


LDFLAGS = $(MCFLAGS) $(OPT) -nostartfiles $(LLIBDIR) \
          -Wl,-Map=$(BUILDDIR)/$(PROJECT).map,--cref,--no-warn-mismatch \
          -Wl,--library-path="$(STARTUPLD_WIN)" \
          -Wl,-T"$(LDS_WIN)"$(LDOPT)


# Generate dependency information
ASFLAGS  += -MD -MP -MF $(DEPDIR)/$(@F).d
ASXFLAGS += -MD -MP -MF $(DEPDIR)/$(@F).d
CFLAGS   += -MD -MP -MF $(DEPDIR)/$(@F).d
CPPFLAGS += -MD -MP -MF $(DEPDIR)/$(@F).d

# Paths where to search for sources
VPATH     = $(SRCPATHS)

#
# Makefile rules
#

all: PRE_MAKE_ALL_RULE_HOOK $(OBJS) $(OUTFILES) POST_MAKE_ALL_RULE_HOOK

PRE_MAKE_ALL_RULE_HOOK:

POST_MAKE_ALL_RULE_HOOK:

$(OBJS): | PRE_MAKE_ALL_RULE_HOOK $(BUILDDIR) $(OBJDIR) $(LSTDIR) $(DEPDIR)

$(BUILDDIR):
ifneq ($(USE_VERBOSE_COMPILE),yes)
	@echo Compiler Options
	@echo $(CC) -c $(CFLAGS) -I. $(IINCDIR) main.c -o main.o
	@echo
endif
	@mkdir -p $(BUILDDIR)

$(OBJDIR):
	@mkdir -p $(OBJDIR)

$(LSTDIR):
	@mkdir -p $(LSTDIR)

$(DEPDIR):
	@mkdir -p $(DEPDIR)

$(CPPOBJS) : $(OBJDIR)/%.o : %.cpp $(MAKEFILE_LIST)
ifeq ($(USE_VERBOSE_COMPILE),yes)
	@echo
	$(CPPC) -c $(CPPFLAGS) -I. $(IINCDIR) $< -o $@
else
	@echo Compiling $(<F)
	@$(CPPC) -c $(CPPFLAGS) -I. $(IINCDIR) $< -o $@
endif

$(CCOBJS) : $(OBJDIR)/%.o : %.cc $(MAKEFILE_LIST)
ifeq ($(USE_VERBOSE_COMPILE),yes)
	@echo
	$(CPPC) -c $(CPPFLAGS) -I. $(IINCDIR) $< -o $@
else
	@echo Compiling $(<F)
	@$(CPPC) -c $(CPPFLAGS) -I. $(IINCDIR) $< -o $@
endif

$(COBJS) : $(OBJDIR)/%.o : %.c $(MAKEFILE_LIST)
ifeq ($(USE_VERBOSE_COMPILE),yes)
	@echo
	$(CC) -c $(CFLAGS) -I. $(IINCDIR) $< -o $@
else
	@echo Compiling $(<F)
	@$(CC) -c $(CFLAGS) -I. $(IINCDIR) $< -o $@
endif

$(ASMOBJS) : $(OBJDIR)/%.o : %.s $(MAKEFILE_LIST)
ifeq ($(USE_VERBOSE_COMPILE),yes)
	@echo
	$(AS) -c $(ASFLAGS) -I. $(IINCDIR) $< -o $@
else
	@echo Compiling $(<F)
	@$(AS) -c $(ASFLAGS) -I. $(IINCDIR) $< -o $@
endif

$(ASMXOBJS) : $(OBJDIR)/%.o : %.S $(MAKEFILE_LIST)
ifeq ($(USE_VERBOSE_COMPILE),yes)
	@echo
	$(CC) -c $(ASXFLAGS) $(TOPT) -I. $(IINCDIR) $< -o $@
else
	@echo Compiling $(<F)
	@$(CC) -c $(ASXFLAGS) $(TOPT) -I. $(IINCDIR) $< -o $@
endif

$(BUILDDIR)/$(PROJECT).elf: $(OBJS) $(LDSCRIPT)
ifeq ($(USE_VERBOSE_COMPILE),yes)
	@echo
	$(LD) $(OBJS) $(LDFLAGS) $(LIBS) -o $@
else
	@echo Linking $@
	@$(LD) $(OBJS) $(LDFLAGS) $(LIBS) -o $@
endif

%.hex: %.elf
ifeq ($(USE_VERBOSE_COMPILE),yes)
	$(HEX) $< $@
else
	@echo Creating $@
	@$(HEX) $< $@
endif

%.bin: %.elf
ifeq ($(USE_VERBOSE_COMPILE),yes)
	$(BIN) $< $@
else
	@echo Creating $@
	@$(BIN) $< $@
endif

%.srec: %.elf
ifdef SREC
  ifeq ($(USE_VERBOSE_COMPILE),yes)
	$(SREC) $< $@
  else
	@echo Creating $@
	@$(SREC) $< $@
  endif
endif

%.dmp: %.elf
ifeq ($(USE_VERBOSE_COMPILE),yes)
	$(OD) $(ODFLAGS) $< > $@
	$(SZ) $<
else
	@echo Creating $@
	@$(OD) $(ODFLAGS) $< > $@
	@echo
	@$(SZ) $<
endif

%.list: %.elf
ifeq ($(USE_VERBOSE_COMPILE),yes)
	$(OD) -S $< > $@
else
	@echo Creating $@
	@$(OD) -S $< > $@
	@echo
	@echo Done
endif

lib: $(OBJS) $(BUILDDIR)/lib$(PROJECT).a

$(BUILDDIR)/lib$(PROJECT).a: $(OBJS)
	@$(AR) -r $@ $^
	@echo
	@echo Done

clean: CLEAN_RULE_HOOK
	@echo Cleaning
	@echo - $(DEPDIR)
	@-rm -fR $(DEPDIR)/* $(BUILDDIR)/* 2>/dev/null
	@-if [ -d "$(DEPDIR)" ]; then rmdir -p --ignore-fail-on-non-empty $(subst ./,,$(DEPDIR)) 2>/dev/null; fi
	@echo - $(BUILDDIR)
	@-if [ -d "$(BUILDDIR)" ]; then rmdir -p --ignore-fail-on-non-empty $(subst ./,,$(BUILDDIR)) 2>/dev/null; fi
	@echo
	@echo Done

CLEAN_RULE_HOOK:

#
# Include the dependency files, should be the last of the makefile
#
-include $(wildcard $(DEPDIR)/*)

# *** EOF ***
