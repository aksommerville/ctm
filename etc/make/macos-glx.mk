# MacOS with GLX.
# This is not a great choice of drivers. It's only available because I had to write GLX anyway for Linux.
# X11 support on the Mac is getting harder as time goes on.
# There is no audio or joystick support this way.

XINC:=-I/usr/X11/include
XBIN:=-L/usr/X11/lib

CC:=gcc -c -MMD -O2 -Isrc -Werror -Wimplicit -Wformat -Wno-parentheses -Wno-pointer-sign $(XINC) -DCTM_ARCH=CTM_ARCH_macos
LD:=gcc $(XBIN)
LDPOST:=-lz -lm -lX11 -lGL

CC+=-DGLSLVERSION=120

OPT:=glx

APP:=CampaignTrailOfTheMummy.app/Contents/MacOS/CampaignTrailOfTheMummy
