# ffaudio tester makefile
# 2020, Simon Zolin

# set OS
ifndef $(OS)
	uname := $(shell uname)
	ifeq ($(uname),Linux)
		OS := linux
	else ifeq ($(uname),FreeBSD)
		OS := freebsd
	else ifeq ($(uname),Darwin)
		OS := apple
	else
		OS := win
	endif
endif

# set compiler
CPREFIX :=
# CPREFIX := x86_64-w64-mingw32-
COMPILER := gcc
ifeq ($(OS),freebsd)
	COMPILER := clang
endif
ifeq ($(OS),apple)
	COMPILER := clang
endif
C := $(CPREFIX)gcc -c
CXX := $(CPREFIX)g++ -c
LINKER := $(CPREFIX)gcc
ifeq ($(COMPILER),clang)
	C := clang -c
	CXX := clang++ -c
	LINKER := clang
endif

# set utils
RM := rm -f

FFBASE_DIR := ../../ffbase
FFAUDIO_DIR := ..
HEADERS := $(wildcard $(FFAUDIO_DIR)/ffaudio/*.h $(FFAUDIO_DIR)/test/*.h)

OUT_DIR := .
TEST_OBJ := \
	$(OUT_DIR)/test.o

TEST_WIN := ffaudio-dsound.exe ffaudio-wasapi.exe
TEST_LINUX := ffaudio-alsa ffaudio-pulse ffaudio-jack
TEST_MAC := ffaudio-coreaudio
TEST_FBSD := ffaudio-oss
ifeq ($(OS),win)
	TEST_DEFAULT := $(TEST_WIN)
else ifeq ($(OS),linux)
	TEST_DEFAULT := $(TEST_LINUX)
else ifeq ($(OS),apple)
	TEST_DEFAULT := $(TEST_MAC)
else ifeq ($(OS),freebsd)
	TEST_DEFAULT := $(TEST_FBSD)
endif

all: $(TEST_DEFAULT)

clean:
	$(RM) $(TEST_LINUX) $(TEST_WIN) $(TEST_MAC) $(TEST_FBSD) $(TEST_OBJ)

TEST_CFLAGS := -I$(FFAUDIO_DIR) -I$(FFBASE_DIR) -Wall -Wextra -Werror -fvisibility=hidden -Wno-unused-parameter
TEST_CFLAGS += -DFF_DEBUG -O0 -g
TEST_CFLAGS += -std=gnu99
# TEST_CFLAGS += -fsanitize=address
# TEST_LDFLAGS += -fsanitize=address
$(OUT_DIR)/%.o: $(FFAUDIO_DIR)/ffaudio/%.c $(HEADERS) $(FFAUDIO_DIR)/test/Makefile
	$(C) $(TEST_CFLAGS) $< -o $@

$(OUT_DIR)/%.o: $(FFAUDIO_DIR)/test/%.c $(HEADERS) $(FFAUDIO_DIR)/test/Makefile
	$(C) $(TEST_CFLAGS) $< -o $@

# ALSA
ffaudio-alsa: $(OUT_DIR)/test.o $(OUT_DIR)/alsa.o
	$(LINKER) $+ -lasound -o $@

# PulseAudio
ffaudio-pulse: $(OUT_DIR)/test.o $(OUT_DIR)/pulse.o
	$(LINKER) $+ -lpulse -o $@

# JACK
ffaudio-jack: $(OUT_DIR)/test.o $(OUT_DIR)/jack.o
	$(LINKER) $+ -ljack -o $@

# DirectSound
ffaudio-dsound.exe: $(OUT_DIR)/test.o $(OUT_DIR)/dsound.o
	$(LINKER) $+ -ldsound -ldxguid -o $@

# WASAPI
ffaudio-wasapi.exe: $(OUT_DIR)/test.o $(OUT_DIR)/wasapi.o
	$(LINKER) $+ -lole32 -o $@

# CoreAudio
ffaudio-coreaudio: $(OUT_DIR)/test.o $(OUT_DIR)/coreaudio.o
	$(LINKER) $+ -framework CoreFoundation -framework CoreAudio -o $@

# OSS
ffaudio-oss: $(OUT_DIR)/test.o $(OUT_DIR)/oss.o
	$(LINKER) $+ -lm -o $@
