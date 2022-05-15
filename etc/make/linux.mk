# Desktop Linux, using GLX, ALSA, and evdev.
# This is potentially less portable than 'linux-sdl', but tends to work better when it works.

CC:=gcc -c -MMD -O2 -Isrc -Werror -Wimplicit -Wformat -DCTM_ARCH=CTM_ARCH_linux -I/usr/include/libdrm -Wno-unused-result
LD:=gcc
LDPOST:=-lz -lm -lasound -lX11 -lGL -lpthread -ldrm -lgbm -lEGL

#TODO Make the ALSA device really configurable, in a config file or something.
ifeq ($(shell uname -n),vcs)
  CC+=-DCTM_ALSA_DEVICE='"hw:0,3"'
endif

OPT:=glx alsa evdev drm

CC+=-DGLSLVERSION=100
