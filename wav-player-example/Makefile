# ffaudio: Makefile for .wav (CD-audio) player
# How to use:
#
#    $ make FFAUDIO_API=alsa
#    $ ./ffaudio-player <file.wav
#
# will build it for ALSA

FFBASE_DIR := ../../ffbase
FFAUDIO_DIR := ..
FFAUDIO_API := # supported values: alsa, pulse, jack, wasapi, dsound, coreaudio, oss
C := gcc -c
LINK := gcc
CFLAGS := -O2 -I$(FFAUDIO_DIR) -I$(FFBASE_DIR)

ifeq "$(FFAUDIO_API)" "alsa"
	FFAUDIO_LINKFLAGS := -lasound
else ifeq "$(FFAUDIO_API)" "pulse"
	FFAUDIO_LINKFLAGS := -lpulse
else ifeq "$(FFAUDIO_API)" "jack"
	FFAUDIO_LINKFLAGS := -ljack
else ifeq "$(FFAUDIO_API)" "wasapi"
	FFAUDIO_LINKFLAGS := -lole32
else ifeq "$(FFAUDIO_API)" "dsound"
	FFAUDIO_LINKFLAGS := -ldsound -ldxguid
else ifeq "$(FFAUDIO_API)" "coreaudio"
	FFAUDIO_LINKFLAGS := -framework CoreFoundation -framework CoreAudio
else ifeq "$(FFAUDIO_API)" "oss"
	FFAUDIO_LINKFLAGS := -lm
endif
FFAUDIO_FILE := $(FFAUDIO_DIR)/ffaudio/$(FFAUDIO_API).c
FFAUDIO_CFLAGS += -DFFAUDIO_INTERFACE_DEFAULT_PTR="&ff$(FFAUDIO_API)"

all: ffaudio-player

ffaudio-api.o: $(FFAUDIO_FILE) $(FFAUDIO_DIR)/ffaudio/audio.h
	$(C) $(CFLAGS) $< -o $@

player.o: ./player.c $(FFAUDIO_DIR)/ffaudio/audio.h
	$(C) $(CFLAGS) $(FFAUDIO_CFLAGS) $< -o $@

ffaudio-player: ./player.o ./ffaudio-api.o
	$(LINK) $+ $(FFAUDIO_LINKFLAGS) -o $@
