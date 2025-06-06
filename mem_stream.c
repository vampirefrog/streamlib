#include <string.h>
#include <errno.h>
#include <stdlib.h>
#ifdef HAVE_GZIP
#include <zlib.h>
#endif

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

static const char *mem_stream_strerror(struct stream *s, int err) {
	(void)s; // Unused parameter
	switch (err) {
		case MEMFS_OK: return "No error";
		case MEMFS_ERR_MALLOC: return "Memory allocation failed";
		case MEMFS_ERR_RESIZE: return "Memory resize failed";
		case MEMFS_ERR_ZLIB_INIT: return "Failed to initialize zlib";
		case MEMFS_ERR_ZLIB_DECOMP: return "Failed to decompress gzip stream";
		case MEMFS_ERR_UNKNOWN: return "Unknown mem_stream error";
		default:
			return strerror(err);
	}
}

static int mem_stream_reserve(struct mem_stream *stream, size_t len) {
	if(stream->data_len + len > (size_t)stream->allocated_len) {
		stream->allocated_len = (stream->data_len + len + 1023) & ~0x3ff;
		stream->data = realloc(stream->data, stream->allocated_len);
		if(!stream->data)
			return MEMFS_ERR_RESIZE;
	}
	return MEMFS_OK;
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

#ifdef HAVE_GZIP
static ssize_t mem_stream_read_gz(struct stream *stream, void *ptr, size_t size) {
	struct mem_stream *mem_stream = (struct mem_stream *)stream;
	mem_stream->z_stream.avail_out = size;
	mem_stream->z_stream.next_out = (Bytef *)ptr;
	int ret = inflate(&mem_stream->z_stream, Z_SYNC_FLUSH);
	int written = mem_stream->z_stream.total_out - mem_stream->position;
	mem_stream->position = mem_stream->z_stream.total_out;
	if(ret == Z_STREAM_END)
		mem_stream->decompressed_data_len = mem_stream->position;
	return written;
}

static ssize_t mem_stream_write_gz(struct stream *stream, const void *ptr, size_t size) {
	(void)stream;
	(void)ptr;
	(void)size;
	return 0;
}

static size_t mem_stream_seek_gz(struct stream *stream, long offset, int whence) {
	(void)stream;
	(void)offset;
	(void)whence;
	return 0;
}

static int mem_stream_eof_gz(struct stream *stream) {
	(void)stream;
	return 0;
}

static long mem_stream_tell_gz(struct stream *stream) {
	(void)stream;
	return -1;
}

static int mem_stream_vprintf_gz(struct stream *stream, const char *fmt, va_list ap) {
	(void)stream;
	(void)fmt;
	(void)ap;
	return 0;
}

static void *mem_stream_get_memory_access_gz(struct stream *stream, size_t *length) {
	(void)stream;
	(void)length;
	return 0;
}

static int mem_stream_revoke_memory_access_gz(struct stream *stream) {
	(void)stream;
	return 1;
}

static int mem_stream_close_gz(struct stream *stream) {
	struct mem_stream *mem_stream = (struct mem_stream *)stream;
	inflateEnd(&mem_stream->z_stream);
	if(mem_stream->allocated_len >= 0) free(mem_stream->data);
	return 0;
}

static int check_gzip_data(uint8_t *data, size_t data_len, size_t *decompressed_data_len) {
	if(data_len < 20) return 0;
	if(data[0] != 0x1f) return 0;
	if(data[1] != 0x8b) return 0;
	if(decompressed_data_len) {
		uint8_t *p = data + data_len - 4;
		*decompressed_data_len = p[0] | p[1] << 8 | p[2] << 16 | p[3] << 24;
	}
	return 1;
}
#endif

int mem_stream_init(struct mem_stream *stream, void *existing_data, size_t existing_data_len, int stream_flags) {
	stream_init(&stream->stream, stream_flags);

	if(existing_data) {
		stream->data = existing_data;
		stream->allocated_len = -1;
		stream->data_len = existing_data_len;
		stream->position = 0;
	} else {
		stream->position = stream->data_len = stream->allocated_len = 0;
		stream->data = 0;
	}
#ifdef HAVE_GZIP
	if(stream_flags & STREAM_TRANSPARENT_GZIP && check_gzip_data(stream->data, stream->data_len, &stream->decompressed_data_len)) {
		stream->z_stream.zalloc = 0;
		stream->z_stream.zfree = 0;
		stream->z_stream.opaque = 0;
		stream->z_stream.avail_in = stream->data_len;
		stream->z_stream.next_in = (z_const Bytef *)stream->data;
		if(inflateInit2(&stream->z_stream, 0x20 | 15) != Z_OK)
			return MEMFS_ERR_ZLIB_INIT;
		stream->stream.write = mem_stream_write_gz;
		stream->stream.read = mem_stream_read_gz;
		stream->stream.seek = mem_stream_seek_gz;
		stream->stream.eof  = mem_stream_eof_gz;
		stream->stream.tell = mem_stream_tell_gz;
		stream->stream.vprintf = mem_stream_vprintf_gz;
		stream->stream.get_memory_access = mem_stream_get_memory_access_gz;
		stream->stream.revoke_memory_access = mem_stream_revoke_memory_access_gz;
		stream->stream.close = mem_stream_close_gz;
		stream->stream.strerror = mem_stream_strerror;
	} else {
#endif
		stream->stream.write = mem_stream_write;
		stream->stream.read = mem_stream_read;
		stream->stream.seek = mem_stream_seek;
		stream->stream.eof  = mem_stream_eof;
		stream->stream.tell = mem_stream_tell;
		stream->stream.vprintf = mem_stream_vprintf;
		stream->stream.get_memory_access = mem_stream_get_memory_access;
		stream->stream.revoke_memory_access = mem_stream_revoke_memory_access;
		stream->stream.close = mem_stream_close;
		stream->stream.strerror = mem_stream_strerror;
#ifdef HAVE_GZIP
	}
#endif
	return MEMFS_OK;
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
