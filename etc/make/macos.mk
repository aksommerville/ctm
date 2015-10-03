# MacOS with SDL.
# In the future, I would like to use native Objective-C APIs and no libSDL.
# But that was out of scope for this project.

SDLC:=$(shell sdl-config --cflags)
SDLLD:=$(patsubst -R%,-L%,$(shell sdl-config --static-libs))

CC:=gcc -c -MMD -O2 -Isrc -Werror -Wimplicit -Wformat -Wno-parentheses -Wno-pointer-sign -DCTM_ARCH=CTM_ARCH_macos $(SDLC)
LD:=gcc $(SDLLD)
LDPOST:=-lz -lm -framework OpenGL

CC+=-DGLSLVERSION=120

OPT:=sdl

APP:=CampaignTrailOfTheMummy.app/Contents/MacOS/CampaignTrailOfTheMummy
POSTRUN:=--fullscreen=1
