# Desktop Linux, using GLX, ALSA, and evdev.
# This is potentially less portable than 'linux-sdl', but tends to work better when it works.

CC:=gcc -c -MMD -O2 -Isrc -Werror -Wimplicit -Wformat -DCTM_ARCH=CTM_ARCH_linux
LD:=gcc
LDPOST:=-lz -lm -lasound -lX11 -lGL

OPT:=glx alsa evdev

CC+=-DGLSLVERSION=100
