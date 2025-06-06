#include <stddef.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
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

static ssize_t zip_file_stream_mem_read(struct stream *stream, void *ptr, size_t size) {
	struct zip_file_stream *zip_file_stream = (struct zip_file_stream *)stream;
	if (zip_file_stream->mem_offset >= zip_file_stream->mem_length)
		return 0;
	size_t remain = zip_file_stream->mem_length - zip_file_stream->mem_offset;
	if (size > remain)
		size = remain;
	memcpy(ptr, (uint8_t*)zip_file_stream->mem_ptr + zip_file_stream->mem_offset, size);
	zip_file_stream->mem_offset += size;
	return size;
}

static size_t zip_file_stream_mem_seek(struct stream *stream, long offset, int whence) {
	struct zip_file_stream *zip_file_stream = (struct zip_file_stream *)stream;
	size_t new_offset = 0;
	switch (whence) {
		case SEEK_SET:
			new_offset = offset;
			break;
		case SEEK_CUR:
			new_offset = zip_file_stream->mem_offset + offset;
			break;
		case SEEK_END:
			new_offset = zip_file_stream->mem_length + offset;
			break;
		default:
			return (size_t)-1;
	}
	if (new_offset > zip_file_stream->mem_length)
		return (size_t)-1;
	zip_file_stream->mem_offset = new_offset;
	return new_offset;
}

static int zip_file_stream_mem_eof(struct stream *stream) {
	struct zip_file_stream *zip_file_stream = (struct zip_file_stream *)stream;
	return zip_file_stream->mem_offset >= zip_file_stream->mem_length;
}

static long zip_file_stream_mem_tell(struct stream *stream) {
	struct zip_file_stream *zip_file_stream = (struct zip_file_stream *)stream;
	return zip_file_stream->mem_offset;
}

static int zip_file_stream_mem_close(struct stream *stream) {
	struct zip_file_stream *zip_file_stream = (struct zip_file_stream *)stream;
	if (zip_file_stream->mem_ptr) {
		free(zip_file_stream->mem_ptr);
		zip_file_stream->mem_ptr = NULL;
	}
	return 0;
}

static const char *zip_file_stream_strerror(struct stream *s, int err) {
	(void)s; // Unused parameter
	switch (err) {
		case ZIPFS_OK: return "No error";
		case ZIPFS_ERR_STAT: return "Failed to stat zip entry";
		case ZIPFS_ERR_OPEN: return "Failed to open zip entry";
		case ZIPFS_ERR_MALLOC: return "Memory allocation failed";
		case ZIPFS_ERR_READ: return "Failed to read from zip entry";
		case ZIPFS_ERR_NOT_GZIP: return "Not a gzip stream";
		case ZIPFS_ERR_ZLIB_INIT: return "Failed to initialize zlib";
		case ZIPFS_ERR_ZLIB_DECOMP: return "Failed to decompress gzip stream";
		case ZIPFS_ERR_MMAP: return "Failed to memory map zip entry";
		case ZIPFS_ERR_UNKNOWN: return "Unknown zip_file_stream error";
		default:
			return "Unknown error";
	}
}

int zip_file_stream_init_index(struct zip_file_stream *stream, zip_t *zip, int index, int stream_flags)  {
	stream_init(&stream->stream, stream_flags);

	int r = zip_stat_index(zip, index, ZIP_STAT_SIZE, &stream->stat);
	stream->stream._errno = zip_error_code_system(zip_get_error(zip));
	if(r) return ZIPFS_ERR_STAT;

	stream->f = zip_fopen_index(zip, index, 0);
	stream->stream._errno = zip_error_code_system(zip_get_error(zip));
	if(!stream->f) return ZIPFS_ERR_OPEN;

#ifdef HAVE_GZIP
	if ((stream_flags & STREAM_ENSURE_MMAP) && (stream_flags & STREAM_TRANSPARENT_GZIP)) {
		// Read the compressed data into z_data
		stream->z_data = malloc(stream->stat.size);
		if(!stream->z_data) return ZIPFS_ERR_MALLOC;
		int rr = zip_fread(stream->f, stream->z_data, stream->stat.size);
		if(rr < 0) {
			free(stream->z_data);
			return ZIPFS_ERR_READ;
		}
		// Check if it's a gzip stream
		if(check_gzip_data(stream->z_data, stream->stat.size, &stream->decompressed_data_len)) {
			// Decompress entire file into memory (mmap-like)
			void *mem = malloc(stream->decompressed_data_len);
			if(!mem) {
				free(stream->z_data);
				return ZIPFS_ERR_MALLOC;
			}
			z_stream zstr;
			memset(&zstr, 0, sizeof(zstr));
			zstr.zalloc = 0;
			zstr.zfree = 0;
			zstr.opaque = 0;
			zstr.avail_in = stream->stat.size;
			zstr.next_in = (z_const Bytef *)stream->z_data;
			zstr.avail_out = stream->decompressed_data_len;
			zstr.next_out = (Bytef *)mem;
			if(inflateInit2(&zstr, 0x20 | 15) != Z_OK) {
				free(stream->z_data);
				free(mem);
				return ZIPFS_ERR_ZLIB_INIT;
			}
			int ret = inflate(&zstr, Z_FINISH);
			inflateEnd(&zstr);
			if(ret != Z_STREAM_END || zstr.total_out != stream->decompressed_data_len) {
				free(stream->z_data);
				free(mem);
				return ZIPFS_ERR_ZLIB_DECOMP;
			}
			free(stream->z_data);
			stream->z_data = NULL;
			stream->mem_ptr = mem;
			stream->mem_length = stream->decompressed_data_len;
			stream->mem_offset = 0;
			stream->stream.read = zip_file_stream_mem_read;
			stream->stream.write = zip_file_stream_write;
			stream->stream.seek = zip_file_stream_mem_seek;
			stream->stream.eof = zip_file_stream_mem_eof;
			stream->stream.tell = zip_file_stream_mem_tell;
			stream->stream.vprintf = zip_file_stream_vprintf;
			stream->stream.get_memory_access = zip_file_stream_get_memory_access;
			stream->stream.revoke_memory_access = zip_file_stream_revoke_memory_access;
			stream->stream.close = zip_file_stream_mem_close;
			stream->stream.strerror = zip_file_stream_strerror;
			return ZIPFS_OK;
		} else {
			free(stream->z_data);
			stream->z_data = NULL;
			// fallback to normal mmap logic below
		}
	}
#endif

	// --- STREAM_ENSURE_MMAP logic ---
	if (stream_flags & STREAM_ENSURE_MMAP) {
		// Read the whole file into memory
		stream->mem_ptr = malloc(stream->stat.size);
		if (!stream->mem_ptr) return ZIPFS_ERR_MALLOC;
		int rr = zip_fseek(stream->f, 0, SEEK_SET);
		if (rr) {
			free(stream->mem_ptr);
			return ZIPFS_ERR_READ;
		}
		rr = zip_fread(stream->f, stream->mem_ptr, stream->stat.size);
		if (rr < 0) {
			free(stream->mem_ptr);
			return ZIPFS_ERR_READ;
		}
		stream->mem_length = stream->stat.size;
		stream->mem_offset = 0;
		stream->stream.read = zip_file_stream_mem_read;
		stream->stream.write = zip_file_stream_write;
		stream->stream.seek = zip_file_stream_mem_seek;
		stream->stream.eof = zip_file_stream_mem_eof;
		stream->stream.tell = zip_file_stream_mem_tell;
		stream->stream.vprintf = zip_file_stream_vprintf;
		stream->stream.get_memory_access = zip_file_stream_get_memory_access;
		stream->stream.revoke_memory_access = zip_file_stream_revoke_memory_access;
		stream->stream.close = zip_file_stream_mem_close;
		stream->stream.strerror = zip_file_stream_strerror;
		return ZIPFS_OK;
	}
	// --- end STREAM_ENSURE_MMAP logic ---

#ifdef HAVE_GZIP
	if(stream_flags & STREAM_TRANSPARENT_GZIP) {
		// Read the compressed data into z_data
		stream->z_data = malloc(stream->stat.size);
		if(!stream->z_data) return ZIPFS_ERR_MALLOC;
		zip_uint64_t bytes_read = zip_fread(stream->f, stream->z_data, stream->stat.size);
		if(bytes_read != stream->stat.size) return ZIPFS_ERR_READ;
		// Check if it's a gzip stream
		if(check_gzip_data(stream->z_data, stream->stat.size, &stream->decompressed_data_len)) {
			// Setup z_stream for decompression
			stream->z_stream.zalloc = 0;
			stream->z_stream.zfree = 0;
			stream->z_stream.opaque = 0;
			stream->z_stream.avail_in = stream->stat.size;
			stream->z_stream.next_in = (z_const Bytef *)stream->z_data;
			stream->z_position = 0;
			if(inflateInit2(&stream->z_stream, 0x20 | 15) != Z_OK)
				return ZIPFS_ERR_ZLIB_INIT;
			// Set up stream methods for gzip decompression
			stream->stream.write = mem_stream_write_gz;
			stream->stream.read = mem_stream_read_gz;
			stream->stream.seek = mem_stream_seek_gz;
			stream->stream.eof  = mem_stream_eof_gz;
			stream->stream.tell = mem_stream_tell_gz;
			stream->stream.vprintf = mem_stream_vprintf_gz;
			stream->stream.get_memory_access = mem_stream_get_memory_access_gz;
			stream->stream.revoke_memory_access = mem_stream_revoke_memory_access_gz;
			stream->stream.close = mem_stream_close_gz;
			stream->stream.strerror = zip_file_stream_strerror;
			return ZIPFS_OK;
		} else {
			free(stream->z_data);
			stream->z_data = NULL;
		}
	}
#endif
	// Default: normal zip file stream
	stream->stream.read = zip_file_stream_read;
	stream->stream.write = zip_file_stream_write;
	stream->stream.seek = zip_file_stream_seek;
	stream->stream.eof = zip_file_stream_eof;
	stream->stream.tell = zip_file_stream_tell;
	stream->stream.vprintf = zip_file_stream_vprintf;
	stream->stream.get_memory_access = zip_file_stream_get_memory_access;
	stream->stream.revoke_memory_access = zip_file_stream_revoke_memory_access;
	stream->stream.close = zip_file_stream_close;
	stream->stream.strerror = zip_file_stream_strerror;

	return ZIPFS_OK;
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
