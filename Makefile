all:
.SILENT:
PRECMD=echo "  $(@F)" ; mkdir -p $(@D) ;

FIND:=/bin/find

#------------------------------------------------------------------------------
# Compile-time configuration.

# Override configuration here, or let us guess.
# Configs only reachable explicity:
#   macos-glx
#   linux-sdl
#   mswin-vialinux
#   mswin-viamacos
#CTM_CONFIG:=

# We normally choose whether to enable audio based on the optional unit selection.
# You can forcibly disable it by defining CTM_AUDIO_DISABLE=1.

ifndef CTM_CONFIG
  UNAMES:=$(shell uname -s)
  ifeq ($(UNAMES),Linux)
    UNAMEM:=$(shell uname -m)
    ifeq ($(UNAMEM),armv6l)
      CTM_CONFIG:=raspi
    else
      CTM_CONFIG:=linux
    endif
  else ifeq ($(UNAMES),Darwin)
    CTM_CONFIG:=macos
  else ifeq ($(firstword $(subst _, ,$(UNAMES))),MINGW32)
    CTM_CONFIG:=mswin
  else ifeq ($(firstword $(subst _, ,$(UNAMES))),MINGW64)
    CTM_CONFIG:=mswin
  else
    $(error Unable to guess configuration)
  endif
endif

APPSFX:=

addl_app_req:

include etc/make/$(CTM_CONFIG).mk

#------------------------------------------------------------------------------
# Generated source files.
# These locate under src.
# Be sure to add them to .gitignore before committing!

GENERATED_FILES:= \
  src/video/ctm_program_icon.c \
  $(patsubst %.shader,%.c,$(wildcard src/video/shaders/*.shader)) \
  src/test/ctm_test_contents.h

src/video/ctm_program_icon.c:etc/tool/mkicon src/video/ctm_program_icon.32x32.rgba;$(PRECMD) $^ $@
src/video/shaders/%.c:etc/tool/mkshader src/video/shaders/%.shader;$(PRECMD) $^ $@
src/test/ctm_test_contents.h:etc/tool/mktests $(shell find src/test -name '*.c');$(PRECMD) $< $@

#------------------------------------------------------------------------------
# Remainder of this file should be fully automatic.

CC+=$(patsubst %,-DCTM_USE_%=1,$(OPT))

MIDDIR:=mid/$(CTM_CONFIG)
OUTDIR:=out/$(CTM_CONFIG)
export OUTDIR

.PRECIOUS:$(GENERATED_FILES)
GENOPTFILES:=$(filter src/opt/%,$(GENERATED_FILES))
GENERATED_FILES:=$(filter-out $(GENOPTFILES),$(GENERATED_FILES)) $(filter $(addprefix src/opt/,$(addsuffix /%,$(OPT))),$(GENOPTFILES))
GENCFILES:=$(filter %.c,$(GENERATED_FILES))
GENHFILES:=$(filter %.h,$(GENERATED_FILES))

CFILES:=$(shell find src -name opt -prune -false -or -name '*.c')
CFILES+=$(shell find $(addprefix src/opt/,$(OPT)) -name '*.c') $(GENCFILES)
OFILES:=$(patsubst src/%.c,$(MIDDIR)/%.o,$(CFILES))
-include $(OFILES:.o=.d)

OFILES_COMMON:=$(filter-out $(MIDDIR)/main/% $(MIDDIR)/editor/% $(MIDDIR)/test/%,$(OFILES))
OFILES_APP:=$(OFILES_COMMON) $(filter $(MIDDIR)/main/%,$(OFILES))
OFILES_EDITOR:=$(OFILES_COMMON) $(filter $(MIDDIR)/editor/%,$(OFILES))
OFILES_TEST:=$(OFILES_COMMON) $(filter $(MIDDIR)/test/%,$(OFILES))

$(MIDDIR)/%.o:src/%.c|$(GENHFILES);$(PRECMD) $(CC) -o $@ $<

ifdef APP
  APP:=$(OUTDIR)/$(APP)
else
  APP:=$(OUTDIR)/ctm$(APPSFX)
endif
export APP
ifdef EDITOR
  EDITOR:=$(OUTDIR)/$(EDITOR)
else
  EDITOR:=$(OUTDIR)/ctm-editor$(APPSFX)
endif
ifdef TEST
  TEST:=$(OUTDIR)/$(TEST)
else
  TEST:=$(OUTDIR)/ctm-test$(APPSFX)
endif

all:$(APP) addl_app_req
$(APP):$(OFILES_APP);$(PRECMD) $(LD) -o $@ $^ $(LDPOST)

all:$(EDITOR) addl_app_req
$(EDITOR):$(OFILES_EDITOR);$(PRECMD) $(LD) -o $@ $^ $(LDPOST)

all:$(TEST) addl_app_req
$(TEST):$(OFILES_TEST);$(PRECMD) $(LD) -o $@ $^ $(LDPOST)

#------------------------------------------------------------------------------
# Special commands.

help:; \
  echo "" ; \
  echo "The Campaign Trail of the Mummy" ; \
  echo "Makefile help" ; \
  echo "" ; \
  echo "Commands:" ; \
  echo "  make [all]                    Build everything for the default configuration." ; \
  echo "  CTM_CONFIG=XXX make [all]     Build for alternate configuration." ; \
  echo "  make clean                    Remove all output and intermediate files." ; \
  echo "  make test                     Build executable and launch it." ; \
  echo "  make edit                     Build editor and launch it." ; \
  echo "  make help                     Print this message." ; \
  echo "  make install                  Build and install (don't run as root)." ; \
  echo "  make uninstall                Remove prior installation." ; \
  echo "" ; \
  echo "Current configuration: $(CTM_CONFIG)" ; \
  echo ""

run:$(APP);$(PRERUN) $(APP) $(POSTRUN)
edit:$(EDITOR);$(EDITOR)
test:$(TEST);$(TEST)

clean:;rm -rf $(GENERATED_FILES) mid out

install:$(APP);CTM_CONFIG=$(CTM_CONFIG) APP=$(APP) etc/tool/install
uninstall:;CTM_CONFIG=$(CTM_CONFIG) APP=$(APP) etc/tool/uninstall
