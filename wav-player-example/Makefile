# ffaudio: Makefile for .wav (CD-audio) player
# How to use:
#
#    $ make FFAUDIO_API=alsa
#    $ ./ffaudio-player <file.wav
#
# will build it for ALSA

FFBASE_DIR := ../../ffbase
FFAUDIO_DIR := ..
FFAUDIO_API := # alsa, pulse, jack, wasapi, dsound, coreaudio, oss
C := gcc -c
LINKER := gcc
CFLAGS := -O2 -I$(FFAUDIO_DIR) -I$(FFBASE_DIR)

FFAUDIO_C := notfound.c
ifeq ($(FFAUDIO_API),alsa)
	FFAUDIO_C := $(FFAUDIO_DIR)/ffaudio/alsa.c
	FFAUDIO_LINKFLAGS := -lasound
endif

ifeq ($(FFAUDIO_API),pulse)
	FFAUDIO_C := $(FFAUDIO_DIR)/ffaudio/pulse.c
	FFAUDIO_LINKFLAGS := -lpulse
endif

ifeq ($(FFAUDIO_API),jack)
	FFAUDIO_C := $(FFAUDIO_DIR)/ffaudio/jack.c
	FFAUDIO_LINKFLAGS := -ljack
endif

ifeq ($(FFAUDIO_API),wasapi)
	FFAUDIO_C := $(FFAUDIO_DIR)/ffaudio/wasapi.c
	FFAUDIO_LINKFLAGS := -lole32
endif

ifeq ($(FFAUDIO_API),dsound)
	FFAUDIO_C := $(FFAUDIO_DIR)/ffaudio/dsound.c
	FFAUDIO_LINKFLAGS := -ldsound -ldxguid
endif

ifeq ($(FFAUDIO_API),coreaudio)
	FFAUDIO_C := $(FFAUDIO_DIR)/ffaudio/coreaudio.c
	FFAUDIO_LINKFLAGS := -framework CoreFoundation -framework CoreAudio
endif

ifeq ($(FFAUDIO_API),oss)
	FFAUDIO_C := $(FFAUDIO_DIR)/ffaudio/oss.c
	FFAUDIO_LINKFLAGS := -lm
endif

all: ffaudio-player

ffaudio-api.o: $(FFAUDIO_C) $(FFAUDIO_DIR)/ffaudio/audio.h
	$(C) $(CFLAGS) $< -o $@

player.o: ./player.c $(FFAUDIO_DIR)/ffaudio/audio.h
	$(C) $(CFLAGS) $< -o $@

ffaudio-player: ./player.o ./ffaudio-api.o
	$(LINKER) $(FFAUDIO_LINKFLAGS) $+ -o $@
