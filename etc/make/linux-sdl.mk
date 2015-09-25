# Desktop Linux, using libSDL.
# This is more portable than plain 'linux', but not as flexible.

SDLC:=$(shell sdl-config --cflags)
SDLLD:=$(shell sdl-config --libs)

CC:=gcc -c -MMD -O2 -Isrc -Werror -Wimplicit -Wformat -Wno-parentheses -Wno-pointer-sign -DCTM_ARCH=CTM_ARCH_linux $(SDLC)
LD:=gcc $(SDLLD)
LDPOST:=-lz -lm -lGL

CC+=-DGLSLVERSION=100

OPT:=sdl
