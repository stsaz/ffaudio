/** ffaudio: tester
2020, Simon Zolin
*/

#include <ffaudio/audio.h>
#include <ffbase/args.h>
#include <ffbase/stringz.h>
#include <test/std.h>
#include <test/test.h>
#ifdef FF_LINUX
#include <time.h>
#endif
typedef unsigned char u_char;

static inline void ffthread_sleep(ffuint msec)
{
#ifdef FF_WIN
	Sleep(msec);
#else
	struct timespec ts = {
		.tv_sec = msec / 1000,
		.tv_nsec = (msec % 1000) * 1000000,
	};
	nanosleep(&ts, NULL);
#endif
}


const ffaudio_interface *audio;
int underrun;
int skip_wav_header;

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

		if (skip_wav_header) {
			static int wav_hdr_skip = 1;
			if (wav_hdr_skip) {
				wav_hdr_skip = 0;
				ffstr_shift(&data, 44);
			}
		}

		while (data.len >= frame_size) {
			ffstdout_fmt("ffaudio.write...");
			r = audio->write(b, data.ptr, data.len);
			if (r == -FFAUDIO_ESYNC) {
				fflog("detected underrun");
				continue;
			}
			if (r < 0)
				fflog("ffaudio.write: %s", audio->error(b));
			else
				fflog(" %dms", r * 1000 / sec_bytes);
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

	fflog("ffaudio.drain #2...");
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

struct conf {
	const char *cmd;
	ffaudio_conf buf;
	ffuint flags;
	ffuint until_ms;
	u_char exclusive;
	u_char hwdev;
	u_char loopback;
	u_char nonblock;
	u_char underrun;
	u_char wav;
};

int conf_format(struct conf *c, ffstr s)
{
	if (ffstr_eqz(&s, "float32"))
		c->buf.format = FFAUDIO_F_FLOAT32;
	else if (ffstr_eqz(&s, "int32"))
		c->buf.format = FFAUDIO_F_INT32;
	else if (ffstr_eqz(&s, "int16"))
		c->buf.format = FFAUDIO_F_INT16;
	else if (ffstr_eqz(&s, "int8"))
		c->buf.format = FFAUDIO_F_INT8;
	else
		return 1;
	return 0;
}

#define O(m)  (void*)(size_t)FF_OFF(struct conf, m)
static const struct ffarg args[] = {
	{ "-buffer",		'u',	O(buf.buffer_length_msec) },
	{ "-channels",		'u',	O(buf.channels) },
	{ "-device",		'=s',	O(buf.device_id) },
	{ "-exclusive",		'1',	O(exclusive) },
	{ "-format",		'u',	conf_format },
	{ "-hwdev",			'1',	O(hwdev) },
	{ "-loopback",		'1',	O(loopback) },
	{ "-nonblock",		'1',	O(nonblock) },
	{ "-rate",			'u',	O(buf.sample_rate) },
	{ "-underrun",		'1',	O(underrun) },
	{ "-until",			'u',	O(until_ms) },
	{ "-wav",			'1',	O(wav) },
	{}
};
#undef O

int conf_read(struct conf *c, int argc, const char **argv)
{
	c->cmd = "";
	if (argc >= 2)
		c->cmd = argv[1];
	if (ffsz_eq(c->cmd, "record"))
		c->flags = FFAUDIO_CAPTURE;
	else if (ffsz_eq(c->cmd, "play"))
		c->flags = FFAUDIO_PLAYBACK;
	else
		return 0;

	struct ffargs a = {};
	int r = ffargs_process_argv(&a, args, c, FFARGS_O_DUPLICATES | FFARGS_O_PARTIAL, (char**)argv + 2, argc - 2);
	if (r) {
		fflog("Error: %s", a.error);
		return 1;
	}

	if (c->loopback) {
		c->flags &= ~0x0f;
		c->flags |= FFAUDIO_LOOPBACK;
	}

	c->flags |= (c->exclusive) ? FFAUDIO_O_EXCLUSIVE : 0;
	c->flags |= (c->hwdev) ? FFAUDIO_O_HWDEV : 0;
	c->flags |= (c->nonblock) ? FFAUDIO_O_NONBLOCK : 0;

	underrun = c->underrun;
	skip_wav_header = c->wav;
	return 0;
}

void help(const char *psname)
{
	ffstdout_fmt(
"%s COMMAND [OPTION...]\n\
COMMAND:\n\
  list     List available devices\n\
  record   Record audio and write to stderr\n\
             e.g. %s record 2>1.raw\n\
  play     Play audio from stdin\n\
             e.g. %s play <1.raw\n\
  help     Show this message\n\
\n\
OPTION:\n\
  -until MSEC     Stop recording after this time (default: 2000)\n\
  -buffer MSEC    Set buffer size in msec (default: 250)\n\
  -format STR     Set sample format: int8, int16, int32, float32 (default: int16)\n\
  -rate N         Set channels number (default: 44100)\n\
  -channels N     Set channels number (default: 2)\n\
  -nonblock       Use non-blocking I/O\n\
  -underrun       Trigger buffer underrun or overrun\n\
  -device STR     Use specific device\n\
  -hwdev          Open \"hw\" device, instead of \"plughw\" (ALSA)\n\
  -exclusive      Open device in exclusive mode (WASAPI)\n\
  -loopback       Open device in loopback mode (WASAPI)\n\
  -wav            Playback: skip WAV header\n\
"
		, psname, psname, psname);
}

int main(int argc, const char **argv)
{
	struct conf conf = {};
	conf.until_ms = 2000;
	conf.buf.app_name = "ffaudio-test";
	conf.buf.buffer_length_msec = 250;
	conf.buf.format = FFAUDIO_F_INT16;
	conf.buf.sample_rate = 44100;
	conf.buf.channels = 2;

	audio = ffaudio_default_interface();

	if (0 != conf_read(&conf, argc, argv))
		return 1;

	ffaudio_init_conf aconf = {};
	aconf.app_name = "ffaudio";
	xieq(0, audio->init(&aconf));

	if (ffsz_eq(conf.cmd, "list"))
		list();

	else if (ffsz_eq(conf.cmd, "record"))
		record(&conf.buf, conf.until_ms, conf.flags);

	else if (ffsz_eq(conf.cmd, "play"))
		play(&conf.buf, conf.flags);

	else // if (ffsz_eq(cmd, "help"))
		help(argv[0]);

	audio->uninit();
	return 0;
}
