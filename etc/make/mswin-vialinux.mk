# Microsoft Windows from my Linux box.
# This has not been tested since the native Windows build was added.

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
