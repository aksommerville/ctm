# Microsoft Windows with MinGW and libSDL.
# You must install libSDL and pthread in your MinGW.

MINGW:=/c/MinGW

INCLUDE:=-I$(MINGW)/include/SDL
LDFLAGS:=$(MINGW)/lib/SDL.dll

CC:=gcc -c -MMD -O2 -Isrc -Werror -Wimplicit -Wformat -DCTM_ARCH=CTM_ARCH_mswin -m32 $(INCLUDE) -DPTW32_STATIC_LIB=1
LD:=gcc -static -m32 $(LDFLAGS)
LDPOST:=-lmingw32 -lSDLmain -lz -lm -lpthreadGC2 -lglew32s -lglu32 -lopengl32 -lglaux

OPT:=sdl
  
APPSFX:=.exe

CC+=-DGLSLVERSION=120

OUTPUT_DLLS:=$(addprefix out/mswin/,SDL.dll)
out/mswin/pthreadGC2.dll:$(MINGW)/bin/pthreadGC2.dll;$(PRECMD) cp $< $@
out/mswin/SDL.dll:$(MINGW)/lib/SDL.dll;$(PRECMD) cp $< $@
$(MINGW)/bin/pthreadGC2.dll:
$(MINGW)/lib/SDL.dll:
addl_app_req:$(OUTPUT_DLLS) out/mswin/ctm-data
out/mswin/ctm-data:;cp -r ctm-data out/mswin/ctm-data
