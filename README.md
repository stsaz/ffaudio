# ffaudio

ffaudio is a fast cross-platform interface for Audio Input/Output for C and C++.

Contents:

* Features
* How to use
	* List all available playback devices
	* Record data from audio device
* How to build
	* Makefile helper
	* Build information for each audio API
* How to test

## Features

* List available playback/capture devices
* Play audio
* Capture audio
* Blocking or non-blocking behaviour for write/drain/read functions
* The most simple API as it can be

Supports:

* ALSA (Linux):
	* "hw" and "plughw" modes
* CoreAudio (macOS)
* DirectSound (Windows)
* JACK (Linux)
* OSS (FreeBSD)
* PulseAudio (Linux)
* WASAPI (Windows):
	* shared and exclusive modes
	* loopback mode (record what you hear)

Note: JACK playback is not implemented.


## How to use

Write your cross-platform code using `ffaudio_interface` interface.

### List all available playback devices

	#include <ffaudio/audio.h>

	// get API
	const ffaudio_interface *audio = ffaudio_default_interface();

	// initialize audio subsystem
	ffaudio_init_conf initconf = {};
	int r = audio->init(&initconf);
	if (r < 0)
		exit(1);

	// enumerate devices one by one
	ffaudio_dev *d = audio->dev_alloc(FFAUDIO_DEV_PLAYBACK);
	for (;;) {
		int r = audio->dev_next(d);
		if (r > 0) {
			break;
		} else if (r < 0) {
			printf("error: %s\n", audio->dev_error(d));
			break;
		}

		printf("device: name: '%s'\n", audio->dev_info(d, FFAUDIO_DEV_NAME));
	}
	audio->dev_free(d);

	audio->uninit();


### Record data from audio device

	#include <ffaudio/audio.h>

	// get API
	const ffaudio_interface *audio = ffaudio_default_interface();

	// initialize audio subsystem
	ffaudio_init_conf initconf = {};
	int r = audio->init(&initconf);
	if (r < 0)
		exit(1);

	// create audio buffer
	ffaudio_buf *buf = audio->alloc();

	// open audio buffer for recording
	ffaudio_conf bufconf = {};
	bufconf.format = FFAUDIO_F_INT16;
	bufconf.sample_rate = 44100;
	bufconf.channels = 2;
	r = audio->open(buf, &bufconf, FFAUDIO_CAPTURE);
	if (r == FFAUDIO_EFORMAT)
		r = audio->open(buf, &bufconf, FFAUDIO_CAPTURE); // open with the supported format
	if (r < 0)
		exit(1);

	// read data from audio buffer and write it to stderr
	for (;;) {
		const void *data;
		r = audio->read(buf, &data);
		if (r < 0)
			exit(1);

		write(2, data.ptr, data.len);
	}

	audio->free(buf);
	audio->uninit();

Instead of calling `ffaudio_default_interface()` which gives you the first available API, you can use a specific API, e.g. `ffalsa`.


## How to build

Include the necessary ffaudio C files into your project's build script, e.g.:

	./ffaudio-alsa.o: $(FFAUDIO_DIR)/ffaudio/alsa.c $(FFAUDIO_DIR)/ffaudio/audio.h
		gcc -c $(CFLAGS) $< -o $@

Use the neceessary linker flags for the audio API, described below.


### Build information for each audio API

ALSA:
* Install `libalsa-devel`
* Compile `ffaudio/alsa.c`
* Link with `-lasound`

PulseAudio:
* Install `libpulse-devel`
* Compile `ffaudio/pulse.c`
* Link with `-lpulse`

JACK:
* Install `jack-audio-connection-kit-devel`
* Compile `ffaudio/jack.c`
* Link with `-ljack`

WASAPI:
* Compile `ffaudio/wasapi.c`
* Link with `-lole32`

DirectSound:
* Compile `ffaudio/dsound.c`
* Link with `-ldsound -ldxguid`

CoreAudio:
* Compile `ffaudio/coreaudio.c`
* Link with `-framework CoreFoundation -framework CoreAudio`

OSS:
* Compile `ffaudio/oss.c`
* Link with `-lm`


## How to test

	git clone https://github.com/stsaz/ffbase
	git clone https://github.com/stsaz/ffaudio
	cd ffaudio
	make

This command builds executable files for all supported audio API on your OS.
There are a lot of additional arguments that you can pass to these executable files.

Linux:

	./ffaudio-alsa list
	./ffaudio-alsa record 2>file.raw
	./ffaudio-alsa play <file.raw

	./ffaudio-pulse list
	./ffaudio-pulse record 2>file.raw
	./ffaudio-pulse play <file.raw

	./ffaudio-jack list
	./ffaudio-jack record 2>file.raw
	# [not implemented] ./ffaudio-jack play <file.raw

Windows:

	.\ffaudio-wasapi.exe list
	.\ffaudio-wasapi.exe record 2>file.raw
	.\ffaudio-wasapi.exe play <file.raw

	.\ffaudio-dsound.exe list
	.\ffaudio-dsound.exe record 2>file.raw
	.\ffaudio-dsound.exe play <file.raw

macOS:

	./ffaudio-coreaudio list
	./ffaudio-coreaudio record 2>file.raw
	./ffaudio-coreaudio play <file.raw

FreeBSD:

	./ffaudio-oss list
	./ffaudio-oss record 2>file.raw
	./ffaudio-oss play <file.raw


## License

This code is absolutely free.


## History

This is actually a refactored code from ff library which is a base for fmedia.  More history can be found in `github.com/stsaz/ff` repo.
