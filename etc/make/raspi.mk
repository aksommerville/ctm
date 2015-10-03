# Raspberry Pi (Raspbian).
# We use low-level APIs: BCM, ALSA, and evdev.

RPIINC:=-I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux
RPIBIN:=-L/opt/vc/lib
RPILIB:=-lGLESv2 -lpthread -lbcm_host -lEGL

CC:=gcc -c -MMD -O2 -Isrc -Werror -Wimplicit -Wformat $(RPIINC) -DCTM_ARCH=CTM_ARCH_raspi
LD:=gcc $(RPIBIN)
LDPOST:=-lz -lm -lasound $(RPILIB)

OPT:=bcm alsa evdev

CC+=-DGLSLVERSION=100
