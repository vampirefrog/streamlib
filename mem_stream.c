#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "mem_stream.h"
#include "util.h"

static ssize_t mem_stream_read(struct stream *stream, void *ptr, size_t size) {
	struct mem_stream *mem_stream = (struct mem_stream *)stream;
	int read_len = MIN(mem_stream->data_len - mem_stream->position, size);
	memcpy(ptr, mem_stream->data + mem_stream->position, read_len);
	mem_stream->position += read_len;
	stream->_errno = errno;
	return read_len;
}

static int mem_stream_reserve(struct mem_stream *stream, size_t len) {
	if(stream->data_len + len > (size_t)stream->allocated_len) {
		stream->allocated_len = (stream->data_len + len + 1023) & ~0x3ff;
		stream->data = realloc(stream->data, stream->allocated_len);
		if(!stream->data)
			return -1;
	}

	return 0;
}

static ssize_t mem_stream_write(struct stream *stream, const void *ptr, size_t size) {
	struct mem_stream *mem_stream = (struct mem_stream *)stream;
	if(mem_stream->position + size > mem_stream->data_len) {
		int err = mem_stream_reserve(mem_stream, mem_stream->position + size - mem_stream->data_len);
		stream->_errno = err;
		if(err) return 0;
		mem_stream->data_len = mem_stream->position + size;
	}
	memcpy(mem_stream->data + mem_stream->position, ptr, size);
	mem_stream->position += size;
	stream->_errno = errno;
	return size;
}

static size_t mem_stream_seek(struct stream *stream, long offset, int whence) {
	struct mem_stream *mem_stream = (struct mem_stream *)stream;
	if(whence == SEEK_SET) {
		mem_stream->position = MIN(offset, (long)mem_stream->data_len);
	} else if(whence == SEEK_CUR) {
		mem_stream->position = MIN(mem_stream->position + offset, mem_stream->data_len);
	} else if(whence == SEEK_END) {
		mem_stream->position = MIN(mem_stream->data_len + offset, mem_stream->data_len);
	}
	stream->_errno = 0;
	return mem_stream->position;
}

static int mem_stream_eof(struct stream *stream) {
	struct mem_stream *mem_stream = (struct mem_stream *)stream;
	return mem_stream->position >= mem_stream->data_len ? 1 : 0;
}

static long mem_stream_tell(struct stream *stream) {
	struct mem_stream *mem_stream = (struct mem_stream *)stream;
	return mem_stream->position;
}

static int mem_stream_vprintf(struct stream *stream, const char *fmt, va_list ap) {
	struct mem_stream *mem_stream = (struct mem_stream *)stream;
	long size = vsnprintf(0, 0, fmt, ap);
	if(mem_stream->position + size > mem_stream->data_len) {
		int err = mem_stream_reserve(mem_stream, mem_stream->position + size - mem_stream->data_len);
		stream->_errno = err;
		if(err) return 0;
		mem_stream->data_len = mem_stream->position + size;
	}
	vsprintf(mem_stream->data + mem_stream->position, fmt, ap);
	mem_stream->position += size;
	stream->_errno = errno;
	return size;
}

static void *mem_stream_get_memory_access(struct stream *stream, size_t *length) {
	struct mem_stream *mem_stream = (struct mem_stream *)stream;
	if(length) *length = mem_stream->data_len;
	return mem_stream->data;
}

static int mem_stream_revoke_memory_access(struct stream *stream) {
	(void)stream;
	// do nothing
	return 0;
}

static int mem_stream_close(struct stream *stream) {
	struct mem_stream *mem_stream = (struct mem_stream *)stream;
	if(mem_stream->allocated_len >= 0) free(mem_stream->data);
	return 0;
}

int mem_stream_init(struct mem_stream *stream, void *existing_data, size_t existing_data_len, int stream_flags) {
	stream_init(&stream->stream, stream_flags);

	if(existing_data) {
		stream->data = existing_data;
		stream->allocated_len = -1;
		stream->data_len = existing_data_len;
	} else {
		stream->position = stream->data_len = stream->allocated_len = 0;
		stream->data = 0;
	}
	stream->stream.write = mem_stream_write;
	stream->stream.read = mem_stream_read;
	stream->stream.seek = mem_stream_seek;
	stream->stream.eof  = mem_stream_eof;
	stream->stream.tell = mem_stream_tell;
	stream->stream.vprintf = mem_stream_vprintf;
	stream->stream.get_memory_access = mem_stream_get_memory_access;
	stream->stream.revoke_memory_access = mem_stream_revoke_memory_access;
	stream->stream.close = mem_stream_close;
	return 0;
}

struct stream *mem_stream_new(void *existing_data, size_t existing_data_len, int stream_flags) {
	struct mem_stream *s = malloc(sizeof(struct mem_stream));
	if(!s) return 0;
	int r = mem_stream_init(s, existing_data, existing_data_len, stream_flags);
	if(r) {
		free(s);
		return 0;
	}
	return &s->stream;
}
