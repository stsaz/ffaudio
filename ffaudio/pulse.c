/** ffaudio: PulseAudio wrapper
2020, Simon Zolin
*/

#include <ffaudio/audio.h>
#include <ffbase/stringz.h>
#include <pulse/pulseaudio.h>
#include <errno.h>


struct pulse_ctx {
	pa_threaded_mainloop *mloop;
	pa_context *ctx;
};

static struct pulse_ctx *gctx;

static void pulse_uninit(struct pulse_ctx *p);
static void pulse_on_connect(pa_context *c, void *userdata);
static void pulse_wait(struct pulse_ctx *p);

int ffpulse_init(ffaudio_init_conf *conf)
{
	struct pulse_ctx *p;

	if (gctx != NULL) {
		conf->error = "already initialized";
		return FFAUDIO_ERROR;
	}

	if (NULL == (p = ffmem_new(struct pulse_ctx))) {
		conf->error = "memory allocate";
		return FFAUDIO_ERROR;
	}

	if (NULL == (p->mloop = pa_threaded_mainloop_new())) {
		conf->error = "pa_threaded_mainloop_new";
		goto end;
	}

	if (conf->app_name == NULL)
		conf->app_name = "ffaudio";

	pa_mainloop_api *mlapi = pa_threaded_mainloop_get_api(p->mloop);
	if (NULL == (p->ctx = pa_context_new_with_proplist(mlapi, conf->app_name, NULL))) {
		conf->error = "pa_context_new_with_proplist";
		goto end;
	}

	pa_context_connect(p->ctx, NULL, 0, NULL);
	pa_context_set_state_callback(p->ctx, pulse_on_connect, p);

	if (0 != pa_threaded_mainloop_start(p->mloop)) {
		conf->error = "pa_threaded_mainloop_start";
		goto end;
	}

	for (;;) {
		int r = pa_context_get_state(p->ctx);
		if (r == PA_CONTEXT_READY)
			break;
		else if (r == PA_CONTEXT_FAILED || r == PA_CONTEXT_TERMINATED) {
			conf->error = pa_strerror(pa_context_errno(p->ctx));
			goto end;
		}

		pulse_wait(p);
	}

	gctx = p;
	return 0;

end:
	pulse_uninit(p);
	return FFAUDIO_ERROR;
}

static void pulse_uninit(struct pulse_ctx *p)
{
	if (p == NULL)
		return;

	if (p->ctx != NULL) {
		pa_threaded_mainloop_lock(p->mloop);
		pa_context_disconnect(p->ctx);
		pa_context_unref(p->ctx);
		pa_threaded_mainloop_unlock(p->mloop);
	}

	if (p->mloop == NULL) {
		pa_threaded_mainloop_stop(p->mloop);
		pa_threaded_mainloop_free(p->mloop);
	}

	ffmem_free(p);
}

void ffpulse_uninit()
{
	pulse_uninit(gctx);
	gctx = NULL;
}

static void pulse_on_connect(pa_context *c, void *userdata)
{
	struct pulse_ctx *p = userdata;
	pa_threaded_mainloop_signal(p->mloop, 0);
}

static void pulse_wait(struct pulse_ctx *p)
{
	pa_threaded_mainloop_wait(p->mloop);
}

static void pulse_op_wait(struct pulse_ctx *p, pa_operation *op)
{
	for (;;) {
		int r = pa_operation_get_state(op);
		if (r == PA_OPERATION_DONE || r == PA_OPERATION_CANCELLED)
			break;
		pulse_wait(p);
	}
}


struct dev_props {
	struct dev_props *next;
	char *id;
	char *name;
};

struct ffaudio_dev {
	ffuint mode;
	struct dev_props *head, *cur;

	const char *errfunc;
	char *errmsg;
	ffuint err;
};

static void pulse_dev_on_next_sink(pa_context *c, const pa_sink_info *info, int eol, void *udata);
static void pulse_dev_on_next_source(pa_context *c, const pa_source_info *info, int eol, void *udata);

ffaudio_dev* ffpulse_dev_alloc(ffuint mode)
{
	ffaudio_dev *d = ffmem_new(ffaudio_dev);
	if (d == NULL)
		return NULL;
	d->mode = mode;
	return d;
}

static void pulse_dev_free_chain(struct dev_props *head)
{
	struct dev_props *it, *next;
	for (it = head;  it != NULL;  it = next) {
		next = it->next;
		ffmem_free(it->id);
		ffmem_free(it->name);
		ffmem_free(it);
	}
}

void ffpulse_dev_free(ffaudio_dev *d)
{
	if (d == NULL)
		return;

	pulse_dev_free_chain(d->head);
	ffmem_free(d->errmsg);
	ffmem_free(d);
}

static void pulse_dev_on_next_sink(pa_context *c, const pa_sink_info *info, int eol, void *udata)
{
	if (eol > 0) {
		pa_threaded_mainloop_signal(gctx->mloop, 0);
		return;
	}

	ffaudio_dev *d = udata;
	struct dev_props *p = ffmem_new(struct dev_props);
	if (p == NULL) {
		d->errfunc = "mem alloc";
		d->err = errno;
		return;
	}
	p->id = ffsz_dup(info->name);
	p->name = ffsz_dup(info->description);
	if (p->id == NULL || p->name == NULL) {
		d->errfunc = "mem alloc";
		d->err = errno;
		ffmem_free(p);
		return;
	}

	if (d->head == NULL)
		d->head = p;
	else
		d->cur->next = p;
	d->cur = p;
}

static void pulse_dev_on_next_source(pa_context *c, const pa_source_info *info, int eol, void *udata)
{
	if (eol > 0) {
		pa_threaded_mainloop_signal(gctx->mloop, 0);
		return;
	}

	ffaudio_dev *d = udata;
	struct dev_props *p = ffmem_new(struct dev_props);
	if (p == NULL) {
		d->errfunc = "mem alloc";
		d->err = errno;
		return;
	}
	p->id = ffsz_dup(info->name);
	p->name = ffsz_dup(info->description);
	if (p->id == NULL || p->name == NULL) {
		d->errfunc = "mem alloc";
		d->err = errno;
		ffmem_free(p);
		return;
	}

	if (d->head == NULL)
		d->head = p;
	else
		d->cur->next = p;
	d->cur = p;
}

int ffpulse_dev_next(ffaudio_dev *d)
{
	if (d->head != NULL) {
		d->cur = d->cur->next;
		if (d->cur == NULL)
			return 1;
		return 0;
	}

	pa_operation *op;
	pa_threaded_mainloop_lock(gctx->mloop);
	if (d->mode == FFAUDIO_DEV_PLAYBACK)
		op = pa_context_get_sink_info_list(gctx->ctx, &pulse_dev_on_next_sink, d);
	else
		op = pa_context_get_source_info_list(gctx->ctx, &pulse_dev_on_next_source, d);

	pulse_op_wait(gctx, op);

	pa_operation_unref(op);
	pa_threaded_mainloop_unlock(gctx->mloop);

	if (d->head == NULL) {
		if (d->err != 0)
			return -FFAUDIO_ERROR;
		return 1;
	}
	d->cur = d->head;
	return 0;
}

const char* ffpulse_dev_info(ffaudio_dev *d, ffuint i)
{
	switch (i) {
	case FFAUDIO_DEV_ID:
		return d->cur->id;
	case FFAUDIO_DEV_NAME:
		return d->cur->name;
	}
	return NULL;
}

const char* ffpulse_dev_error(ffaudio_dev *d)
{
	ffmem_free(d->errmsg);
	d->errmsg = ffsz_allocfmt("%s: %d", d->errfunc, d->err);
	return d->errmsg;
}


struct ffaudio_buf {
	struct pulse_ctx *ctx;
	pa_stream *stm;
	ffuint buf_locked;
	ffuint nonblock;
	ffuint drained;
	pa_operation *drain_op;
	const char *error;
};

ffaudio_buf* ffpulse_alloc()
{
	ffaudio_buf *b = ffmem_new(ffaudio_buf);
	if (b == NULL)
		return NULL;
	return b;
}

void ffpulse_free(ffaudio_buf *b)
{
	if (b == NULL)
		return;

	if (b->stm != NULL) {
		pa_stream_disconnect(b->stm);
		pa_stream_unref(b->stm);
	}
	if (b->drain_op != NULL)
		pa_operation_unref(b->drain_op);
	ffmem_free(b);
}

static const ffushort afmt[] = {
	FFAUDIO_F_INT16,
	FFAUDIO_F_INT24,
	FFAUDIO_F_INT32,
	FFAUDIO_F_FLOAT32,
};
static const ffuint afmt_pa[] = {
	PA_SAMPLE_S16LE,
	PA_SAMPLE_S24LE,
	PA_SAMPLE_S32LE,
	PA_SAMPLE_FLOAT32LE,
};

/** ffaudio format -> Pulse format */
static int pulse_fmt(ffuint f)
{
	int r;
	if (0 > (r = ffarrint16_find(afmt, FF_COUNT(afmt), f)))
		return -FFAUDIO_F_INT16;
	return afmt_pa[r];
}

static void pulse_on_io(pa_stream *s, ffsize nbytes, void *udata);

/** msec -> bytes:
rate*width*channels*msec/1000 */
static ffuint buffer_size(const ffaudio_conf *conf, ffuint msec)
{
	return conf->sample_rate * (conf->format & 0xff) / 8 * conf->channels * msec / 1000;
}

int ffpulse_open(ffaudio_buf *b, ffaudio_conf *conf, ffuint flags)
{
	int r = FFAUDIO_ERROR;
	b->nonblock = !!(flags & FFAUDIO_O_NONBLOCK);
	b->ctx = gctx;

	if (conf->buffer_length_msec == 0)
		conf->buffer_length_msec = 500;

	if (conf->app_name == NULL)
		conf->app_name = "ffaudio";

	if (0 > (r = pulse_fmt(conf->format))) {
		conf->format = -r;
		return FFAUDIO_EFORMAT;
	}

	pa_threaded_mainloop_lock(b->ctx->mloop);

	pa_sample_spec spec;
	spec.format = r;
	spec.rate = conf->sample_rate;
	spec.channels = conf->channels;
	b->stm = pa_stream_new(b->ctx->ctx, conf->app_name, &spec, NULL);
	if (b->stm == NULL) {
		b->error = "pa_stream_new";
		goto end;
	}

	pa_buffer_attr attr;
	ffmem_fill(&attr, 0xff, sizeof(pa_buffer_attr));
	attr.tlength = buffer_size(conf, conf->buffer_length_msec);

	if ((flags & 0x0f) == FFAUDIO_DEV_PLAYBACK) {
		pa_stream_set_write_callback(b->stm, pulse_on_io, b);
		pa_stream_connect_playback(b->stm, conf->device_id, &attr, 0, NULL, NULL);
	} else {
		pa_stream_set_read_callback(b->stm, pulse_on_io, b);
		pa_stream_connect_record(b->stm, conf->device_id, &attr, 0);
	}

	for (;;) {
		r = pa_stream_get_state(b->stm);
		if (r == PA_STREAM_READY)
			break;
		else if (r == PA_STREAM_FAILED) {
			b->error = pa_strerror(pa_context_errno(b->ctx->ctx));
			goto end;
		}

		pulse_wait(b->ctx);
	}

	r = 0;

end:
	pa_threaded_mainloop_unlock(b->ctx->mloop);
	return r;
}

static void pulse_on_op(pa_stream *s, int success, void *udata)
{
	struct pulse_ctx *p = udata;
	pa_threaded_mainloop_signal(p->mloop, 0);
}

int pulse_start(ffaudio_buf *b)
{
	if (!pa_stream_is_corked(b->stm))
		return 0;

	pa_operation *op = pa_stream_cork(b->stm, 0, pulse_on_op, b->ctx);
	pulse_op_wait(b->ctx, op);
	pa_operation_unref(op);
	return 0;
}

int ffpulse_start(ffaudio_buf *b)
{
	pa_threaded_mainloop_lock(b->ctx->mloop);
	pulse_start(b);
	pa_threaded_mainloop_unlock(b->ctx->mloop);
	return 0;
}

int ffpulse_stop(ffaudio_buf *b)
{
	pa_threaded_mainloop_lock(b->ctx->mloop);

	if (pa_stream_is_corked(b->stm))
		goto end;

	pa_operation *op = pa_stream_cork(b->stm, 1, pulse_on_op, b->ctx);
	pulse_op_wait(b->ctx, op);
	pa_operation_unref(op);

end:
	pa_threaded_mainloop_unlock(b->ctx->mloop);
	return 0;
}

int ffpulse_clear(ffaudio_buf *b)
{
	pa_threaded_mainloop_lock(b->ctx->mloop);

	pa_operation *op = pa_stream_flush(b->stm, pulse_on_op, b->ctx);
	pulse_op_wait(b->ctx, op);
	pa_operation_unref(op);

	pa_threaded_mainloop_unlock(b->ctx->mloop);
	return 0;
}

static int pulse_writeonce(ffaudio_buf *b, const void *data, ffsize len)
{
	int r;
	ffsize n;
	void *buf;

	n = pa_stream_writable_size(b->stm);
	if (n == 0)
		return 0;

	r = pa_stream_begin_write(b->stm, &buf, &n);
	if (r < 0 || buf == NULL) {
		b->error = "pa_stream_begin_write";
		return -FFAUDIO_ERROR;
	}
	n = ffmin(len, n);

	ffmem_copy(buf, data, n);

	if (0 != pa_stream_write(b->stm, buf, n, NULL, 0, PA_SEEK_RELATIVE)) {
		b->error = "pa_stream_write";
		return -FFAUDIO_ERROR;
	}

	return n;
}

static void pulse_on_io(pa_stream *s, ffsize nbytes, void *udata)
{
	ffaudio_buf *b = udata;
	pa_threaded_mainloop_signal(b->ctx->mloop, 0);
}

static int pulse_readonce(ffaudio_buf *b, const void **data)
{
	if (b->buf_locked) {
		b->buf_locked = 0;
		pa_stream_drop(b->stm);
	}

	ffsize len;
	if (0 != pa_stream_peek(b->stm, data, &len)) {
		b->error = "pa_stream_peek";
		return -FFAUDIO_ERROR;
	}
	b->buf_locked = 1;

	if (*data == NULL && len != 0) {
		b->error = "data holes are not supported";
		return -FFAUDIO_ERROR;
	}

	return len;
}

int ffpulse_write(ffaudio_buf *b, const void *data, ffsize len)
{
	int r;
	pa_threaded_mainloop_lock(b->ctx->mloop);

	for (;;) {
		r = pulse_writeonce(b, data, len);
		if (r != 0)
			goto end;

		pulse_start(b);

		if (b->nonblock)
			goto end;

		pulse_wait(b->ctx);
	}

end:
	pa_threaded_mainloop_unlock(b->ctx->mloop);
	return r;
}

int ffpulse_drain(ffaudio_buf *b)
{
	if (b->drained)
		return 1;

	pa_threaded_mainloop_lock(b->ctx->mloop);

	int r;
	if (b->drain_op != NULL) {
		r = pa_operation_get_state(b->drain_op);
		if (r == PA_OPERATION_DONE || r == PA_OPERATION_CANCELLED) {
			pa_operation_unref(b->drain_op);
			b->drain_op = NULL;
			b->drained = 1;
			r = 1;
		} else {
			pulse_start(b);
			r = 0;
		}

		goto end;
	}

	pulse_start(b);

	pa_operation *op = pa_stream_drain(b->stm, pulse_on_op, b->ctx);
	if (!b->nonblock) {
		pulse_op_wait(b->ctx, op);
		pa_operation_unref(op);
		r = 1;
	} else {
		b->drain_op = op;
		r = 0;
	}

end:
	pa_threaded_mainloop_unlock(b->ctx->mloop);
	return r;
}

int ffpulse_read(ffaudio_buf *b, const void **data)
{
	int r;
	pa_threaded_mainloop_lock(b->ctx->mloop);

	for (;;) {
		r = pulse_readonce(b, data);
		if (r != 0)
			goto end;

		if (b->nonblock)
			goto end;

		pulse_wait(b->ctx);
	}

end:
	pa_threaded_mainloop_unlock(b->ctx->mloop);
	return r;
}

/*
Note: libpulse's code calls _exit() when it fails to allocate a memory buffer (/src/pulse/xmalloc.c) */
const char* ffpulse_error(ffaudio_buf *b)
{
	return b->error;
}


const struct ffaudio_interface ffpulse = {
	ffpulse_init,
	ffpulse_uninit,

	ffpulse_dev_alloc,
	ffpulse_dev_free,
	ffpulse_dev_error,
	ffpulse_dev_next,
	ffpulse_dev_info,

	ffpulse_alloc,
	ffpulse_free,
	ffpulse_error,
	ffpulse_open,
	ffpulse_start,
	ffpulse_stop,
	ffpulse_clear,
	ffpulse_write,
	ffpulse_drain,
	ffpulse_read,
};
