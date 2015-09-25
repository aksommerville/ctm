# Microsoft Windows from my Mac.
# This has not been tested since the native Windows build was added.

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
