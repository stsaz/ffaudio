# ffaudio tester makefile
# 2020, Simon Zolin

# Set OS
ifndef OS
	uname := $(shell uname)
	ifeq "$(uname)" "Linux"
		OS := linux
	else ifeq "$(uname)" "FreeBSD"
		OS := freebsd
	else ifeq "$(uname)" "Darwin"
		OS := apple
	endif
else ifeq "$(OS)" "Windows_NT"
	# OS=Windows_NT is default env var on Windows
	OS := windows
endif

# Set compiler
COMPILER := gcc
ifeq "$(OS)" "freebsd"
	COMPILER := clang
endif
ifeq "$(OS)" "apple"
	COMPILER := clang
endif

CROSS_PREFIX :=
ifndef CROSS_PREFIX
	ifeq "$(OS)" "windows"
		CROSS_PREFIX := x86_64-w64-mingw32-
	endif
endif

C := $(CROSS_PREFIX)gcc -c -pipe
CXX := $(CROSS_PREFIX)g++ -c -pipe
LINK := $(CROSS_PREFIX)gcc -pipe
ifeq "$(COMPILER)" "clang"
	C := clang -c
	CXX := clang++ -c
	LINK := clang
endif

# set utils
RM := rm -f

FFBASE_DIR := ../../ffbase
FFAUDIO_DIR := ..
HEADERS := $(wildcard $(FFAUDIO_DIR)/ffaudio/*.h $(FFAUDIO_DIR)/test/*.h)

OUT_DIR := .

TEST_WIN := ffaudio-dsound.exe ffaudio-wasapi.exe
TEST_LINUX := ffaudio-alsa ffaudio-pulse
TEST_MAC := ffaudio-coreaudio
TEST_FBSD := ffaudio-oss

ifeq ($(FFAUDIO_API),alsa)
	FFAUDIO_LINKFLAGS := -lasound
else ifeq ($(FFAUDIO_API),pulse)
	FFAUDIO_LINKFLAGS := -lpulse
else ifeq ($(FFAUDIO_API),jack)
	FFAUDIO_LINKFLAGS := -ljack
else ifeq ($(FFAUDIO_API),wasapi)
	FFAUDIO_LINKFLAGS := -lole32
else ifeq ($(FFAUDIO_API),dsound)
	FFAUDIO_LINKFLAGS := -ldsound -ldxguid
else ifeq ($(FFAUDIO_API),coreaudio)
	FFAUDIO_LINKFLAGS := -framework CoreFoundation -framework CoreAudio
else ifeq ($(FFAUDIO_API),oss)
	FFAUDIO_LINKFLAGS := -lm
endif
FFAUDIO_CFLAGS += -DFFAUDIO_INTERFACE_DEFAULT_PTR="&ff$(FFAUDIO_API)"

TEST_CFLAGS := -I$(FFAUDIO_DIR) -I$(FFBASE_DIR) -Wall -Wextra -Werror -fvisibility=hidden -Wno-unused-parameter
TEST_CFLAGS += -DFF_DEBUG -O0 -g
TEST_CFLAGS += -std=gnu99
# TEST_CFLAGS += -fsanitize=address
# TEST_LDFLAGS += -fsanitize=address

TEST_OBJ := \
	$(OUT_DIR)/$(FFAUDIO_API).o \
	$(OUT_DIR)/test.o

all: ffaudio-$(FFAUDIO_API)

clean:
	$(RM) $(TEST_LINUX) $(TEST_WIN) $(TEST_MAC) $(TEST_FBSD) $(TEST_OBJ)

$(OUT_DIR)/%.o: $(FFAUDIO_DIR)/ffaudio/%.c $(HEADERS) $(FFAUDIO_DIR)/test/Makefile
	$(C) $(TEST_CFLAGS) $< -o $@
$(OUT_DIR)/%.o: $(FFAUDIO_DIR)/test/%.c $(HEADERS) $(FFAUDIO_DIR)/test/Makefile
	$(C) $(TEST_CFLAGS) $(FFAUDIO_CFLAGS) $< -o $@

ffaudio-$(FFAUDIO_API): $(TEST_OBJ)
	$(LINK) $+ $(FFAUDIO_LINKFLAGS) -o $@
