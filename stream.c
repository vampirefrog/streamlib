#include <string.h>
#include <stdlib.h>

#include "stream.h"

void stream_init(struct stream *stream) {
	memset(stream, 0, sizeof(*stream));
}

ssize_t stream_read(struct stream *stream, void *ptr, size_t size) {
	return stream->read(stream, ptr, size);
}

ssize_t stream_write(struct stream *stream, const void *ptr, size_t size) {
	return stream->write(stream, ptr, size);
}

size_t stream_seek(struct stream *stream, long offset, int whence) {
	return stream->seek(stream, offset, whence);
}

int stream_eof(struct stream *stream) {
	return stream->eof(stream);
}

long stream_tell(struct stream *stream) {
	return stream->tell(stream);
}

int stream_printf(struct stream *stream, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int r = stream->vprintf(stream, fmt, ap);
	va_end(ap);
	return r;
}

void *stream_get_memory_access(struct stream *stream, size_t *length) {
	return stream->get_memory_access(stream, length);
}

int stream_revoke_memory_access(struct stream *stream) {
	return stream->revoke_memory_access(stream);
}

int stream_close(struct stream *stream) {
	return stream->close(stream);
}

int stream_destroy(struct stream *stream) {
	int r = stream_close(stream);
	free(stream);
	return r;
}

uint8_t stream_read_uint8(struct stream *stream) {
	uint8_t r;
	stream_read(stream, &r, 1);
	return r;
}

uint16_t stream_read_big_uint16(struct stream *stream) {
	uint8_t buf[2];
	stream_read(stream, buf, 2);
	return buf[0] << 16 | buf[1];
}

uint32_t stream_read_big_uint32(struct stream *stream) {
	uint8_t buf[4];
	stream_read(stream, buf, 4);
	return buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];
}

ssize_t stream_write_big_uint16(struct stream *stream, uint16_t i) {
	uint8_t buf[2] = { i >> 8, i & 0xff };
	return stream_write(stream, buf, 2);
}

ssize_t stream_write_big_uint32(struct stream *stream, uint32_t i) {
	uint8_t buf[4] = { i >> 24, i >> 16, i >> 8, i };
	return stream_write(stream, buf, 4);
}

int stream_read_compare(struct stream *stream, const void *data, size_t len) {
	if(!len) len = strlen((char *)data);
	void *buf = malloc(len);
	if(!buf) return 0;
	int ret = !(stream_read(stream, buf, len) < (ssize_t)len || memcmp(buf, data, len));
	free(buf);
	return ret;
}

