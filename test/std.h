/** ffos: standard I/O
2020, Simon Zolin
*/

#pragma once

/*
ffstdin_read
ffstdout_write ffstderr_write
ffstdout_fmt ffstderr_fmt
fflog
*/

#include <ffbase/string.h>
#include <ffbase/unicode.h>


#ifdef FF_WIN

static inline ffssize ffstdin_read(void *buf, ffsize cap)
{
	DWORD read;
	HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
	if (!ReadFile(h, buf, cap, &read, 0))
		return -1;
	return read;
}

static inline ffssize _ffstd_write(HANDLE h, const void *data, ffsize len)
{
	DWORD written;
	if (!WriteFile(h, data, len, &written, 0))
		return -1;
	return written;
}

static inline ffssize ffstdout_write(const void *data, ffsize len)
{
	return _ffstd_write(GetStdHandle(STD_OUTPUT_HANDLE), data, len);
}

static inline ffssize ffstderr_write(const void *data, ffsize len)
{
	return _ffstd_write(GetStdHandle(STD_ERROR_HANDLE), data, len);
}

#else

static inline ffssize ffstdin_read(void *buf, ffsize cap)
{
	return read(0, buf, cap);
}

static inline ffssize ffstdout_write(const void *data, ffsize len)
{
	return write(1, data, len);
}

static inline ffssize ffstderr_write(const void *data, ffsize len)
{
	return write(2, data, len);
}

#endif

/** Read from stdin */
static ffssize ffstdin_read(void *buf, ffsize cap);

/** Write to stdout */
static ffssize ffstdout_write(const void *data, ffsize len);

/** Write to stderr */
static ffssize ffstderr_write(const void *data, ffsize len);

/** %-formatted output to stdout
NOT printf()-compatible (see ffs_formatv()) */
static inline ffssize ffstdout_fmt(const char *fmt, ...)
{
	char s[4096];
	va_list args;
	va_start(args, fmt);
	ffssize r = ffs_formatv(s, sizeof(s), fmt, args);
	va_end(args);
	if (r <= 0)
		return r;
	return ffstdout_write(s, r);
}

/** %-formatted output to stderr
NOT printf()-compatible (see ffs_formatv()) */
static inline ffssize ffstderr_fmt(const char *fmt, ...)
{
	char s[4096];
	va_list args;
	va_start(args, fmt);
	ffssize r = ffs_formatv(s, sizeof(s), fmt, args);
	va_end(args);
	if (r <= 0)
		return r;
	return ffstderr_write(s, r);
}

#define fflog(fmt, ...)  (void) ffstdout_fmt(fmt "\n", ##__VA_ARGS__)
