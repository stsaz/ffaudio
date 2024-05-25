# ffaudio tester makefile

ROOT_DIR := ../..
FFBASE_DIR := $(ROOT_DIR)/ffbase
FFAUDIO_DIR := $(ROOT_DIR)/ffaudio

include $(FFBASE_DIR)/conf.mk
ifeq "$(SYS)" "android"
	NDK_DIR := $(SDK_DIR)/ndk/$(NDK_VER)
	include $(FFAUDIO_DIR)/test/andk.mk
endif

TEST_ANDROID := ffaudio-aaudio
TEST_WIN := ffaudio-dsound.exe ffaudio-wasapi.exe
TEST_LINUX := ffaudio-alsa ffaudio-pulse
TEST_MAC := ffaudio-coreaudio
TEST_FBSD := ffaudio-oss

ifeq "$(FFAUDIO_API)" "aaudio"
	FFAUDIO_LINKFLAGS := -laaudio
else ifeq "$(FFAUDIO_API)" "alsa"
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
FFAUDIO_CFLAGS += -DFFAUDIO_INTERFACE_DEFAULT_PTR="&ff$(FFAUDIO_API)"

CFLAGS += $(A_CFLAGS)
CFLAGS += -MMD -MP -I$(FFAUDIO_DIR) -I$(FFBASE_DIR) \
	-std=gnu99 \
	-Wall -Wextra -Wno-unused-parameter -Wno-multichar \
	-fvisibility=hidden
CFLAGS += -DFF_DEBUG -O0 -g
LINKFLAGS += $(A_LINKFLAGS)
# CFLAGS += -fsanitize=address
# LINKFLAGS += -fsanitize=address

OBJ := \
	$(FFAUDIO_API).o \
	test.o

default: ffaudio-$(FFAUDIO_API)

clean:
	$(RM) $(TEST_LINUX) $(TEST_WIN) $(TEST_MAC) $(TEST_FBSD) $(OBJ)

-include $(wildcard *.d)

%.o: $(FFAUDIO_DIR)/ffaudio/%.c
	$(C) $(CFLAGS) $< -o $@
%.o: $(FFAUDIO_DIR)/test/%.c
	$(C) $(CFLAGS) $(FFAUDIO_CFLAGS) $< -o $@

ffaudio-$(FFAUDIO_API): $(OBJ)
	$(LINK) $+ $(LINKFLAGS) $(FFAUDIO_LINKFLAGS) -o $@
