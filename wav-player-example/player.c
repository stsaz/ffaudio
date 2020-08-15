/** ffaudio: .wav (CD-audio) player
2020, Simon Zolin
*/

#include <ffaudio/audio.h>
#include <ffbase/string.h>
#include <test/std.h>
#include <assert.h>

int main(int argc, const char **argv)
{
	ffaudio_conf bufconf = {};
	bufconf.app_name = "ffaudio-player";
	bufconf.format = FFAUDIO_F_INT16;
	bufconf.sample_rate = 44100;
	bufconf.channels = 2;

	const ffaudio_interface *audio = ffaudio_default_interface();
	assert(audio != NULL);

	ffaudio_init_conf conf = {};
	conf.app_name = "ffaudio-player";
	assert(0 == audio->init(&conf));

	ffaudio_buf *b = audio->alloc();
	assert(b != NULL);

	int r = audio->open(b, &bufconf, FFAUDIO_PLAYBACK);
	if (r == FFAUDIO_EFORMAT)
		fflog("Default device doesn't support CD-audio format");
	else if (r != 0)
		fflog("ffaudio.open: %d: %s", r, audio->error(b));
	assert(r == 0);

	ffuint frame_size = bufconf.channels * (bufconf.format & 0xff) / 8;
	ffuint sec_bytes = bufconf.sample_rate * bufconf.channels * (bufconf.format & 0xff) / 8;
	ffuint cap = sec_bytes;
	void *buffer = ffmem_alloc(cap);
	ffstr data;
	ffstr_set(&data, buffer, 0);

	for (;;) {
		ffssize n = ffstdin_read(data.ptr, cap - data.len);
		if (n == 0)
			break;
		assert(n >= 0);
		data.len += n;

		static int wav_hdr_skip = 1;
		if (wav_hdr_skip) {
			wav_hdr_skip = 0;
			ffstr_shift(&data, 44);
		}

		while (data.len >= frame_size) {
			r = audio->write(b, data.ptr, data.len);
			if (r < 0)
				fflog("ffaudio.write: %s", audio->error(b));
			assert(r >= 0);
			ffstr_shift(&data, r);
		}

		ffmem_move(buffer, data.ptr, data.len);
		data.ptr = buffer + data.len;
	}

	audio->drain(b);
	audio->free(b);
	ffmem_free(buffer);
	audio->uninit();
	return 0;
}
