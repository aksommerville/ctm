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

# We normally choose whether to enable audio based on the optional unit selection.
# You can forcibly disable it by defining CTM_AUDIO_DISABLE=1.

ifeq ($(CTM_CONFIG),raspi) # ----- Linux on Raspberry Pi -----

  RPIINC:=-I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux
  RPIBIN:=-L/opt/vc/lib
  RPILIB:=-lGLESv2 -lpthread -lbcm_host -lEGL

  CC:=gcc -c -MMD -O2 -Isrc -Werror -Wimplicit -Wformat $(RPIINC) -DCTM_ARCH=CTM_ARCH_raspi
  LD:=gcc $(RPIBIN)
  LDPOST:=-lz -lm -lasound $(RPILIB)

  OPT:=bcm alsa evdev

  CC+=-DGLSLVERSION=100

else ifeq ($(CTM_CONFIG),linux) # ----- Desktop Linux with GLX/ALSA/evdev -----

  CC:=gcc -c -MMD -O2 -Isrc -Werror -Wimplicit -Wformat -DCTM_ARCH=CTM_ARCH_linux
  LD:=gcc
  LDPOST:=-lz -lm -lasound -lX11 -lGL

  OPT:=glx alsa evdev

  CC+=-DGLSLVERSION=100
  
else ifeq ($(CTM_CONFIG),linux-sdl) # ----- Desktop Linux with SDL (much more portable) -----

  SDLC:=$(shell sdl-config --cflags)
  SDLLD:=$(shell sdl-config --libs)

  CC:=gcc -c -MMD -O2 -Isrc -Werror -Wimplicit -Wformat -Wno-parentheses -Wno-pointer-sign -DCTM_ARCH=CTM_ARCH_linux $(SDLC)
  LD:=gcc $(SDLLD)
  LDPOST:=-lz -lm -lGL

  CC+=-DGLSLVERSION=100

  OPT:=sdl

else ifeq ($(CTM_CONFIG),macos) # ----- MacOS X with SDL (default) ----- 

  SDLC:=$(shell sdl-config --cflags)
  SDLLD:=$(patsubst -R%,-L%,$(shell sdl-config --static-libs))

  CC:=gcc -c -MMD -O2 -Isrc -Werror -Wimplicit -Wformat -Wno-parentheses -Wno-pointer-sign -DCTM_ARCH=CTM_ARCH_macos $(SDLC)
  LD:=gcc $(SDLLD)
  LDPOST:=-lz -lm -lGL

  CC+=-DGLSLVERSION=120

  OPT:=sdl

  APP:=CampaignTrailOfTheMummy.app/Contents/MacOS/CampaignTrailOfTheMummy
  POSTRUN:=--fullscreen=1

else ifeq ($(CTM_CONFIG),macos-glx) # ----- MacOS X with GLX (must request explicitly) -----
# This is not a great choice of drivers. It's only available because I had to write GLX anyway for Linux.
# There is no audio or joystick support this way.

  XINC:=-I/usr/X11/include
  XBIN:=-L/usr/X11/lib

  CC:=gcc -c -MMD -O2 -Isrc -Werror -Wimplicit -Wformat -Wno-parentheses -Wno-pointer-sign $(XINC) -DCTM_ARCH=CTM_ARCH_macos
  LD:=gcc $(XBIN)
  LDPOST:=-lz -lm -lX11 -lGL

  CC+=-DGLSLVERSION=120

  OPT:=glx

  APP:=CampaignTrailOfTheMummy.app/Contents/MacOS/CampaignTrailOfTheMummy

else ifeq ($(CTM_CONFIG),mswin) # ----- Microsoft Windows with MinGW -----

  INCLUDE:=-I/c/MinGW/include/SDL
  LDFLAGS:=/c/MinGW/lib/SDL.dll

  CC:=gcc -c -MMD -O2 -Isrc -Werror -Wimplicit -Wformat -DCTM_ARCH=CTM_ARCH_mswin -m32 $(INCLUDE) -DPTW32_STATIC_LIB=1
  LD:=gcc -static -m32 $(LDFLAGS)
  LDPOST:=-lmingw32 -lSDLmain -lz -lm -lpthreadGC2 -lglew32s -lglu32 -lopengl32 -lglaux

  OPT:=sdl
  
  APPSFX:=.exe

  CC+=-DGLSLVERSION=120

  OUTPUT_DLLS:=$(addprefix out/mswin/,SDL.dll)
  out/mswin/pthreadGC2.dll:/c/MinGW/bin/pthreadGC2.dll;$(PRECMD) cp $< $@
  out/mswin/SDL.dll:/c/MinGW/lib/SDL.dll;$(PRECMD) cp $< $@
  /c/MinGW/bin/pthreadGC2.dll:
  /c/MinGW/lib/SDL.dll:
  addl_app_req:$(OUTPUT_DLLS) out/mswin/ctm-data
  out/mswin/ctm-data:;cp -r ctm-data out/mswin/ctm-data

else ifeq ($(CTM_CONFIG),mswin-vialinux) # ----- Microsoft Windows, cross-compiled from my Linux box. -----

  GCCPFX:=i586-mingw32msvc-
  
  SDLC:=$(shell /usr/i586-mingw32msvc/bin/sdl-config --cflags)
  SDLLD:=$(shell /usr/i586-mingw32msvc/bin/sdl-config --libs)

  CC:=$(GCCPFX)gcc -c -MMD -O2 -Isrc -Werror -Wimplicit -Wformat -DCTM_ARCH=CTM_ARCH_mswin $(SDLC)
  LD:=$(GCCPFX)gcc
  LDPOST:=$(SDLLD) -lglew32s -lopengl32 -lzlib1 -lm -lpthreadGC2

  OPT:=sdl
  
  APPSFX:=.exe
  POSTRUN:=& etc/tool/watchstdio

  CC+=-DGLSLVERSION=130

else ifeq ($(CTM_CONFIG),mswin-viamacos) # ----- Microsoft Windows, cross-compiled from my Mac. -----

  GCCPFX:=i386-mingw32-
  
  SDLC:=$(shell /usr/local/i386-mingw32-4.3.0/i386-mingw32/bin/sdl-config --cflags)
  SDLLD:=$(shell /usr/local/i386-mingw32-4.3.0/i386-mingw32/bin/sdl-config --libs)

  CC:=$(GCCPFX)gcc -c -MMD -O2 -Isrc -Werror -Wimplicit -Wformat -DCTM_ARCH=CTM_ARCH_mswin $(SDLC)
  LD:=$(GCCPFX)gcc 
  LDPOST:=$(SDLLD) -lglew32s -lopengl32 -lzlib1 -lm -lpthreadGC2

  OPT:=sdl
  
  APPSFX:=.exe
  POSTRUN:=& etc/tool/watchstdio
  PRERUN:=wine

  CC+=-DGLSLVERSION=120

else
  $(error Unexpected value '$(CTM_CONFIG)' for CTM_CONFIG)
endif

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
