/** ffaudio: tester
2020, Simon Zolin
*/

#include <ffaudio/audio.h>
#include <ffbase/stringz.h>
#include <test/std.h>
#include <test/test.h>


static inline void ffthread_sleep(ffuint msec)
{
#ifdef FF_WIN
	Sleep(msec);
#else
	usleep(msec * 1000);
#endif
}


const ffaudio_interface *audio;
int underrun;

void list()
{
	ffaudio_dev *d;

	// FFAUDIO_DEV_PLAYBACK, FFAUDIO_DEV_CAPTURE
	static const char* const mode[] = { "playback", "capture" };
	for (ffuint i = 0;  i != 2;  i++) {
		fflog("%s devices:", mode[i]);
		d = audio->dev_alloc(i);
		x(d != NULL);

		for (;;) {
			int r = audio->dev_next(d);
			if (r > 0)
				break;
			else if (r < 0) {
				fflog("error: %s", audio->dev_error(d));
				break;
			}

			fflog("device: name: '%s'  id: '%s'  default: %s"
				, audio->dev_info(d, FFAUDIO_DEV_NAME)
				, audio->dev_info(d, FFAUDIO_DEV_ID)
				, audio->dev_info(d, FFAUDIO_DEV_IS_DEFAULT)
				);
		}

		audio->dev_free(d);
	}
}

void record(ffaudio_conf *conf, ffuint until_ms, ffuint flags)
{
	int r;
	ffaudio_buf *b;
	b = audio->alloc();
	x(b != NULL);

	ffstdout_fmt("ffaudio.open...");
	r = audio->open(b, conf, flags);
	if (r == FFAUDIO_EFORMAT) {
		ffstdout_fmt(" reopening...");
		r = audio->open(b, conf, flags);
	}
	if (r != 0)
		fflog("ffaudio.open: %d: %s", r, audio->error(b));
	xieq(0, r);
	fflog(" %d/%d/%d %dms"
		, conf->format, conf->sample_rate, conf->channels
		, conf->buffer_length_msec);

	ffuint msec_bytes = conf->sample_rate * conf->channels * (conf->format & 0xff) / 8 / 1000;
	ffstr data = {};

	ffuint total = 0;
	ffuint next_overrun = 1000*msec_bytes;
	for (;;) {
		ffstdout_fmt("ffaudio.read...");
		r = audio->read(b, (const void**)&data.ptr);
		fflog(" %dms", r / msec_bytes);
		if (r < 0)
			fflog("ffaudio.read: %s", audio->error(b));
		x(r >= 0);
		data.len = r;

		ffstderr_write(data.ptr, data.len);
		total += r;
		if (total >= msec_bytes * until_ms)
			break;

		if (underrun && total >= next_overrun) {
			fflog("trigger overrun");
			ffthread_sleep(1000);
			next_overrun += 1000*msec_bytes;
		}
	}

	fflog("ffaudio.stop...");
	r = audio->stop(b);
	if (r != 0)
		fflog("ffaudio.stop: %s", audio->error(b));
	xieq(0, r);

	fflog("ffaudio.clear...");
	r = audio->clear(b);
	if (r != 0)
		fflog("ffaudio.clear: %s", audio->error(b));
	xieq(0, r);

	fflog("ffaudio.start...");
	r = audio->start(b);
	if (r != 0)
		fflog("ffaudio.start: %s", audio->error(b));
	xieq(0, r);

	audio->free(b);
	fflog("record done");
}

void play(ffaudio_conf *conf, ffuint flags)
{
	int r;
	ffaudio_buf *b;

	b = audio->alloc();
	x(b != NULL);

	ffstdout_fmt("ffaudio.open...");
	r = audio->open(b, conf, flags);
	if (r == FFAUDIO_EFORMAT)
		r = audio->open(b, conf, flags);
	if (r != 0)
		fflog("ffaudio.open: %d: %s", r, audio->error(b));
	xieq(0, r);
	fflog(" %d/%d/%d %dms"
		, conf->format, conf->sample_rate, conf->channels
		, conf->buffer_length_msec);

	ffuint frame_size = conf->channels * (conf->format & 0xff) / 8;
	ffuint sec_bytes = conf->sample_rate * conf->channels * (conf->format & 0xff) / 8;
	ffuint cap = sec_bytes;
	void *buffer = ffmem_alloc(cap);
	ffstr data;
	ffstr_set(&data, buffer, 0);

	ffuint total = 0;
	ffuint total_written = 0;
	ffuint next_underrun = sec_bytes;
	for (;;) {
		ffssize n = ffstdin_read(data.ptr, cap - data.len);
		if (n == 0)
			break;
		x(n >= 0);
		data.len += n;
		total += n;

#if 0
		static int wav_hdr_skip = 1;
		if (wav_hdr_skip) {
			wav_hdr_skip = 0;
			ffstr_shift(&data, 44);
		}
#endif

		while (data.len >= frame_size) {
			ffstdout_fmt("ffaudio.write...");
			r = audio->write(b, data.ptr, data.len);
			if (r == -FFAUDIO_ESYNC) {
				fflog("detected underrun");
				continue;
			}
			fflog(" %dms", r * 1000 / sec_bytes);
			if (r < 0)
				fflog("ffaudio.write: %s", audio->error(b));
			x(r >= 0);
			ffstr_shift(&data, r);
			total_written += r;

			if (underrun && total_written >= next_underrun) {
				fflog("trigger underrun");
				ffthread_sleep(1000);
				next_underrun += sec_bytes;
			}
		}

		ffmem_move(buffer, data.ptr, data.len);
		data.ptr = buffer + data.len;
	}

	// noop
	r = audio->start(b);
	if (r != 0)
		fflog("ffaudio.start: %s", audio->error(b));
	xieq(0, r);

	fflog("ffaudio.drain...");
	for (;;) {
		r = audio->drain(b);
		if (r < 0)
			fflog("ffaudio.drain: %s", audio->error(b));
		if (r != 0)
			break;
	}
	x(r == 1);

	// ffaudio.drain() #2
	fflog("ffaudio.drain...");
	r = audio->drain(b);
	if (r < 0)
		fflog("ffaudio.drain: %s", audio->error(b));
	x(r == 1);

	r = audio->stop(b);
	if (r != 0)
		fflog("ffaudio.stop: %s", audio->error(b));
	xieq(0, r);

	r = audio->clear(b);
	if (r != 0)
		fflog("ffaudio.clear: %s", audio->error(b));
	xieq(0, r);

	audio->free(b);
	ffmem_free(buffer);
	fflog("play done");
}

int main(int argc, const char **argv)
{
	const char *cmd = "";
	if (argc >= 2)
		cmd = argv[1];

	ffaudio_conf bufconf = {};
	bufconf.app_name = "ffaudio-test";
	bufconf.buffer_length_msec = 250;
	bufconf.format = FFAUDIO_F_INT16;
	bufconf.sample_rate = 44100;
	bufconf.channels = 2;

	audio = ffaudio_default_interface();

	ffuint until_ms = 2000;
	ffuint flags = 0;

	if (ffsz_eq(cmd, "record"))
		flags = FFAUDIO_CAPTURE;
	else if (ffsz_eq(cmd, "play"))
		flags = FFAUDIO_PLAYBACK;

	for (int i = 2;  i < argc;  i++) {
		const char *v = argv[i];
		ffstr s;
		ffstr_setz(&s, v);

		if (ffstr_eqz(&s, "--exclusive")) {
			flags |= FFAUDIO_O_EXCLUSIVE;

		} else if (ffstr_eqz(&s, "--loopback")) {
			flags &= ~0x0f;
			flags |= FFAUDIO_LOOPBACK;

		} else if (ffstr_eqz(&s, "--hwdev")) {
			flags |= FFAUDIO_O_HWDEV;

		} else if (ffstr_eqz(&s, "--nonblock")) {
			flags |= FFAUDIO_O_NONBLOCK;

		} else if (ffstr_eqz(&s, "--underrun")) {
			underrun = 1;

		} else if (ffstr_matchz(&s, "--buffer=")) {
			ffstr_shift(&s, FFS_LEN("--buffer="));
			ffuint n;
			if (!ffstr_to_uint32(&s, &n)) {
				fflog("bad value: %s", v);
				return 1;
			}
			bufconf.buffer_length_msec = n;

		} else if (ffstr_matchz(&s, "--until=")) {
			ffstr_shift(&s, FFS_LEN("--until="));
			ffuint n;
			if (!ffstr_to_uint32(&s, &n)) {
				fflog("bad value: %s", v);
				return 1;
			}
			until_ms = n;

		} else if (ffstr_matchz(&s, "--channels=")) {
			ffstr_shift(&s, FFS_LEN("--channels="));
			ffuint n;
			if (!ffstr_to_uint32(&s, &n)) {
				fflog("bad value: %s", v);
				return 1;
			}
			bufconf.channels = n;

		} else if (ffstr_matchz(&s, "--rate=")) {
			ffstr_shift(&s, FFS_LEN("--rate="));
			ffuint n;
			if (!ffstr_to_uint32(&s, &n)) {
				fflog("bad value: %s", v);
				return 1;
			}
			bufconf.sample_rate = n;

		} else if (ffstr_matchz(&s, "--format=")) {
			ffstr_shift(&s, FFS_LEN("--format="));
			if (ffstr_eqz(&s, "float32"))
				bufconf.format = FFAUDIO_F_FLOAT32;
			else if (ffstr_eqz(&s, "int32"))
				bufconf.format = FFAUDIO_F_INT32;
			else if (ffstr_eqz(&s, "int16"))
				bufconf.format = FFAUDIO_F_INT16;
			else if (ffstr_eqz(&s, "int8"))
				bufconf.format = FFAUDIO_F_INT8;
			else {
				fflog("bad value: %s", v);
				return 1;
			}

		} else if (ffstr_matchz(&s, "--device=")) {
			ffstr_shift(&s, FFS_LEN("--device="));
			bufconf.device_id = ffsz_dupn(s.ptr, s.len);

		} else {
			fflog("unknown option: %s", v);
			return 1;
		}
	}

	ffaudio_init_conf conf = {};
	conf.app_name = "ffaudio";
	xieq(0, audio->init(&conf));

	if (ffsz_eq(cmd, "list"))
		list();

	else if (ffsz_eq(cmd, "record"))
		record(&bufconf, until_ms, flags);

	else if (ffsz_eq(cmd, "play"))
		play(&bufconf, flags);

	else // if (ffsz_eq(cmd, "help"))
		ffstdout_fmt(
"%s COMMAND [OPTION...]\n\
COMMAND:\n\
  list     list available devices\n\
  record   record audio and write to stderr\n\
             e.g. %s record 2>1.raw\n\
  play     play audio from stdin\n\
             e.g. %s play <1.raw\n\
  help     show this message\n\
\n\
OPTION:\n\
  --until=MSEC   stop recording after this time (default:2000)\n\
  --buffer=MSEC  set buffer size in msec (default:250)\n\
  --format=...   set sample format: int8, int16, int32, float32 (default:int16)\n\
  --rate=N       set channels number (default:44100)\n\
  --channels=N   set channels number (default:2)\n\
  --nonblock     use non-blocking I/O\n\
  --underrun     trigger buffer underrun or overrun\n\
  --device=...   use specific device\n\
  --hwdev        open \"hw\" device, instead of \"plughw\" (ALSA)\n\
  --exclusive    open device in exclusive mode (WASAPI)\n\
  --loopback     open device in loopback mode (WASAPI)\n\
"
			, argv[0], argv[0], argv[0]);

	audio->uninit();
	return 0;
}
