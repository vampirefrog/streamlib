#include <stddef.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdlib.h>
#ifdef HAVE_GZIP
#include <zlib.h>
#endif

#include "zip_file_stream.h"

#ifdef HAVE_LIBZIP
static ssize_t zip_file_stream_read(struct stream *stream, void *ptr, size_t size) {
	struct zip_file_stream *zip_file_stream = (struct zip_file_stream *)stream;
	size_t r = zip_fread(zip_file_stream->f, ptr, size);
	stream->_errno = zip_error_code_system(zip_file_get_error(zip_file_stream->f));
	return r;
}

static ssize_t zip_file_stream_write(struct stream *stream, const void *ptr, size_t size) {
	(void)stream;
	(void)ptr;
	(void)size;
	return 0;
}

static size_t zip_file_stream_seek(struct stream *stream, long offset, int whence) {
	struct zip_file_stream *zip_file_stream = (struct zip_file_stream *)stream;
	size_t r = zip_fseek(zip_file_stream->f, offset, whence);
	stream->_errno = zip_error_code_system(zip_file_get_error(zip_file_stream->f));
	return r;
}

static int zip_file_stream_eof(struct stream *stream) {
	struct zip_file_stream *zip_file_stream = (struct zip_file_stream *)stream;
	zip_int64_t t = zip_ftell(zip_file_stream->f);
	if(t < 0) return t;
	return t == (zip_int64_t)zip_file_stream->stat.size;
}

static long zip_file_stream_tell(struct stream *stream) {
	struct zip_file_stream *zip_file_stream = (struct zip_file_stream *)stream;
	return zip_ftell(zip_file_stream->f);
}

static int zip_file_stream_vprintf(struct stream *stream, const char *fmt, va_list ap) {
	(void)stream;
	(void)fmt;
	(void)ap;
	return -1;
}

static void *zip_file_stream_get_memory_access(struct stream *stream, size_t *length) {
	struct zip_file_stream *zip_file_stream = (struct zip_file_stream *)stream;
	stream->mem = malloc(zip_file_stream->stat.size);
	if(!stream->mem) return 0;
	int r = zip_fseek(zip_file_stream->f, 0, SEEK_SET);
	if(r) {
		free(stream->mem);
		return 0;
	}
	r = zip_fread(zip_file_stream->f, stream->mem, zip_file_stream->stat.size);
	if(r < 0) {
		free(stream->mem);
		return 0;
	}
	if(length) *length = zip_file_stream->stat.size;
	return stream->mem;
}

static int zip_file_stream_revoke_memory_access(struct stream *stream) {
	if(stream->mem) free(stream->mem);
	return 0;
}

static int zip_file_stream_close(struct stream *stream) {
	struct zip_file_stream *zip_file_stream = (struct zip_file_stream *)stream;
	int r = zip_fclose(zip_file_stream->f);
	stream->_errno = r;
	return r;
}

#ifdef HAVE_GZIP
static ssize_t mem_stream_read_gz(struct stream *stream, void *ptr, size_t size) {
	struct zip_file_stream *zip_file_stream = (struct zip_file_stream *)stream;
	zip_file_stream->z_stream.avail_out = size;
	zip_file_stream->z_stream.next_out = (Bytef *)ptr;
	int ret = inflate(&zip_file_stream->z_stream, Z_SYNC_FLUSH);
	int written = zip_file_stream->z_stream.total_out - zip_file_stream->z_position;
	zip_file_stream->z_position = zip_file_stream->z_stream.total_out;
	if(ret == Z_STREAM_END)
		zip_file_stream->decompressed_data_len = zip_file_stream->z_position;
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
	struct zip_file_stream *zip_file_stream = (struct zip_file_stream *)stream;
	inflateEnd(&zip_file_stream->z_stream);
	free(zip_file_stream->z_data);
	return 0;
}
#endif

int zip_file_stream_init_index(struct zip_file_stream *stream, zip_t *zip, int index, int stream_flags)  {
	stream_init(&stream->stream, stream_flags);

	int r = zip_stat_index(zip, index, ZIP_STAT_SIZE, &stream->stat);
	stream->stream._errno = zip_error_code_system(zip_get_error(zip));
	if(r) return r;

	stream->f = zip_fopen_index(zip, index, 0);
	stream->stream._errno = zip_error_code_system(zip_get_error(zip));
	if(!stream->f) return -1;

#ifdef HAVE_GZIP
	if(stream_flags & STREAM_TRANSPARENT_GZIP) {
		stream->z_data = malloc(stream->stat.size);
		if(!stream->z_data) return -3;
		zip_uint64_t bytes_read = zip_fread(stream->f, stream->z_data, stream->stat.size);
		if(bytes_read != stream->stat.size) return -4;

		stream->z_stream.zalloc = 0;
		stream->z_stream.zfree = 0;
		stream->z_stream.opaque = 0;
		stream->z_stream.avail_in = stream->stat.size;
		stream->z_stream.next_in = (z_const Bytef *)stream->z_data;
		stream->z_position = 0;
		if(inflateInit2(&stream->z_stream, 0x20 | 15) != Z_OK)
			return -5;
		stream->stream.write = mem_stream_write_gz;
		stream->stream.read = mem_stream_read_gz;
		stream->stream.seek = mem_stream_seek_gz;
		stream->stream.eof  = mem_stream_eof_gz;
		stream->stream.tell = mem_stream_tell_gz;
		stream->stream.vprintf = mem_stream_vprintf_gz;
		stream->stream.get_memory_access = mem_stream_get_memory_access_gz;
		stream->stream.revoke_memory_access = mem_stream_revoke_memory_access_gz;
		stream->stream.close = mem_stream_close_gz;
	} else {
#endif
		stream->stream.read = zip_file_stream_read;
		stream->stream.write = zip_file_stream_write;
		stream->stream.seek = zip_file_stream_seek;
		stream->stream.eof = zip_file_stream_eof;
		stream->stream.tell = zip_file_stream_tell;
		stream->stream.vprintf = zip_file_stream_vprintf;
		stream->stream.get_memory_access = zip_file_stream_get_memory_access;
		stream->stream.revoke_memory_access = zip_file_stream_revoke_memory_access;
		stream->stream.close = zip_file_stream_close;
#ifdef HAVE_GZIP
	}
#endif

	return 0;
}

struct stream *zip_file_stream_create_index(zip_t *zip, int index, int stream_flags) {
	struct zip_file_stream *s = malloc(sizeof(struct zip_file_stream));
	if(!s) return 0;
	int r = zip_file_stream_init_index(s, zip, index, stream_flags);
	if(r) {
		free(s);
		return 0;
	}
	return (struct stream *)s;
}
#endif
