#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#ifdef HAVE_LIBZIP
#include <zip.h>
#endif

// stream init flags
#define STREAM_ENSURE_REWIND            (1 <<  1)
#define STREAM_ENSURE_SEEK_SET          (1 <<  2)
#define STREAM_ENSURE_SEEK_CUR_FORWARD  (1 <<  3)
#define STREAM_ENSURE_SEEK_CUR_BACKWARD (1 <<  4)
#define STREAM_ENSURE_SEEK_END          (1 <<  5)
#define STREAM_ENSURE_FAST_SEEK         (1 <<  6)
#define STREAM_ENSURE_MMAP              (1 <<  7)
#ifdef HAVE_GZIP
#define STREAM_TRANSPARENT_GZIP         (1 <<  8)
#endif

// stream info flags
#define STREAM_IS_GZIPPED               (1 << 16)
#define STREAM_CAN_READ                 (1 << 17)
#define STREAM_CAN_WRITE                (1 << 18)
#define STREAM_CAN_REWIND               (1 << 19)
#define STREAM_CAN_SEEK_SET             (1 << 19)
#define STREAM_CAN_SEEK_CUR_FORWARD     (1 << 20)
#define STREAM_CAN_SEEK_CUR_BACKWARD    (1 << 21)
#define STREAM_CAN_SEEK_END             (1 << 22)
#define STREAM_CAN_TELL                 (1 << 23)
#define STREAM_CAN_EOF                  (1 << 24)
#define STREAM_CAN_MMAP                 (1 << 25)
#define STREAM_IS_MMAPPED               (1 << 26)

struct stream {
	int _errno;
	void *mem;
	size_t mem_size;
	int flags;

	ssize_t (*read)(struct stream *, void *ptr, size_t size);
	ssize_t (*write)(struct stream *, const void *ptr, size_t size);
	size_t (*seek)(struct stream *, long offset, int whence);
	int (*eof)(struct stream *);
	long (*tell)(struct stream *);
	int (*vprintf)(struct stream *, const char *fmt, va_list ap);
	void *(*get_memory_access)(struct stream *, size_t *length);
	int (*revoke_memory_access)(struct stream *);
	int (*close)(struct stream *);
	const char *(*strerror)(struct stream *, int err);
};

void stream_init(struct stream *stream, int flags);
ssize_t stream_read(struct stream *, void *ptr, size_t size);
ssize_t stream_write(struct stream *stream, const void *ptr, size_t size);
size_t stream_seek(struct stream *stream, long offset, int whence);
int stream_eof(struct stream *stream);
long stream_tell(struct stream *stream);
void *stream_get_memory_access(struct stream *stream, size_t *length);
int stream_revoke_memory_access(struct stream *stream);
int stream_close(struct stream *stream);
int stream_destroy(struct stream *stream);
uint8_t stream_read_uint8(struct stream *stream);
uint16_t stream_read_big_uint16(struct stream *stream);
uint32_t stream_read_big_uint32(struct stream *stream);
ssize_t stream_write_uint8(struct stream *stream, uint8_t i);
ssize_t stream_write_big_uint16(struct stream *stream, uint16_t i);
ssize_t stream_write_big_uint32(struct stream *stream, uint32_t i);
int stream_printf(struct stream *stream, const char *fmt, ...);
int stream_read_compare(struct stream *stream, const void *data, size_t len);
const char *stream_strerror(struct stream *stream, int err);
