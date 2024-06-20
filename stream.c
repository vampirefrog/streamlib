#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <dirent.h>
#include <sys/stat.h>
#include "stream.h"
#include "tools.h"

/* Wrapper functions */
int stream_read(struct stream *stream, void *ptr, size_t size) {
	return stream->read(stream, ptr, size);
}

ssize_t stream_write(struct stream *stream, void *ptr, size_t size) {
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
	return stream->stream_get_memory_access(stream, length);
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
	stream->_errno = errno;
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

ssize_t stream_write_buffer(struct stream *stream, struct buffer *buffer) {
	return stream_write(stream, buffer->data, buffer->data_len);
}

int stream_read_compare(struct stream *stream, const void *data, int len) {
	if(!len) len = strlen((char *)data);
	uint8_t *buf = malloc(len);
	if(buf) {
		int ret = !(stream_read(stream, buf, len) < len || memcmp(buf, data, len));
		free(buf);
		return ret;
	}
	return 0;
}

static size_t mem_stream_read(struct stream *stream, void *ptr, size_t size) {
	struct mem_stream *mem_stream = (struct mem_stream *)stream;
	int read_len = MIN(mem_stream->buffer->data_len - stream->position, size);
	memcpy(ptr, mem_stream->buffer->data + stream->position, read_len);
	stream->position += read_len;
	stream->_errno = errno;
	return read_len;
}

static size_t mem_stream_write(struct stream *stream, void *ptr, size_t size) {
	struct mem_stream *mem_stream = (struct mem_stream *)stream;
	if(mem_stream->buffer->data_len < stream->position + size) {
		int err = buffer_reserve(mem_stream->buffer, stream->position + size - mem_stream->buffer->data_len);
		stream->_errno = errno;
		if(err) return 0;
		mem_stream->buffer->data_len += stream->position + size - mem_stream->buffer->data_len;
	}
	memcpy(mem_stream->buffer->data + stream->position, ptr, size);
	stream->position += size;
	stream->_errno = errno;
	return size;
}

static size_t mem_stream_seek(struct stream *stream, long offset, int whence) {
	struct mem_stream *mem_stream = (struct mem_stream *)stream;
	stream->position = MIN(offset, mem_stream->buffer->data_len);
	stream->_errno = 0;
	return stream->position;
}

static int mem_stream_eof(struct stream *stream) {
	struct mem_stream *mem_stream = (struct mem_stream *)stream;
	stream->position = stream->position >= mem_stream->buffer->data_len;
	return stream->position;
}

static long mem_stream_tell(struct stream *stream) {
	return stream->position;
}

static int mem_vprintf(struct stream *stream, const char *fmt, va_list ap) {
	struct mem_stream *mem_stream = (struct mem_stream *)stream;
	int l = vsnprintf(0, 0, fmt, ap);
	char *cur_end = mem_stream->data + mem_stream->position;
	int g = mem_stream_grow(mem_stream, l);
	if(g) return g;
	// UNFINISHED
}

static void mem_get_memory_access(struct stream *stream, size_t *length) {
	struct mem_stream *mem_stream = (struct mem_stream *)stream;
	if(length) *length = mem_stream->data_len;
	return mem_stream->data;
}

static void mem_revoke_memory_access(struct stream *) {
	// do nothing
}

static int mem_close(struct stream *stream) {
	struct mem_stream *mem_stream = (struct mem_stream *)stream;
	if(mem_stream->allocated_len >= 0) free(mem_stream->data);
	return 0;
}

int mem_stream_init(struct mem_stream *stream, struct buffer *buffer) {
	if(!buffer)
		return 1;

	stream->buffer = buffer;
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

struct stream *mem_stream_create() {
	struct mem_stream *s = malloc(sizeof(struct mem_stream));
	if(!s) return 0;
	int r = mem_stream_init(s, filename, mode);
	if(r) {
		free(s);
		return 0;
	}
	return &s.stream;
}

static size_t file_stream_read(struct stream *stream, void *ptr, size_t size) {
	struct file_stream *file_stream = (struct file_stream *)stream;
	size_t r = fread(ptr, 1, size, file_stream->f);
	stream->_errno = errno;
	return r;
}

static size_t file_stream_write(struct stream *stream, void *ptr, size_t size) {
	struct file_stream *write_stream = (struct file_stream *)stream;
	size_t r = fwrite(ptr, 1, size, write_stream->f);
	stream->_errno = errno;
	return r;
}

static size_t file_stream_seek(struct stream *stream, long offset, int whence) {
	struct file_stream *file_stream = (struct file_stream *)stream;
	size_t r = fseek(file_stream->f, offset, whence);
	stream->_errno = errno;
	return r;
}

static int file_stream_eof(struct stream *stream) {
	struct file_stream *file_stream = (struct file_stream *)stream;
	int r = feof(file_stream->f);
	stream->_errno = errno;
	return r;
}

static long file_stream_tell(struct stream *stream) {
	struct file_stream *file_stream = (struct file_stream *)stream;
	long r = ftell(file_stream->f);
	stream->_errno = errno;
	return r;
}

static int file_stream_vprintf(struct stream *stream, const char *fmt, va_list ap) {
	struct file_stream *file_stream = (struct file_stream *)stream;
	return vprintf(file_stream->f, fmt, ap);
}

static void file_stream_get_memory_access(struct stream *stream, size_t *length) {
	struct file_stream *file_stream = (struct file_stream *)stream;

	int fd = fileno(file_stream->f);
	if(fd == -1) {
		stream->_errno = errno;
		return 0;
	}

	// Get file size using fstat
	struct stat st;
	if(fstat(fd, &st) == -1) {
		stream->_errno = errno;
		return 0;
	}
	if(length) *length = st.st_size;

	// Map file into memory using mmap
	stream->mem_size = st.st_size;
	stream->mem = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if(stream->mem == MAP_FAILED) {
		stream->_errno = errno;
		return 0;
	}

	return stream->mem;
}

static void file_stream_revoke_memory_access(struct stream *stream) {
	munmap(stream->mem, stream->mem_size);
}

static int file_stream_close(struct stream *stream) {
	struct file_stream *file_stream = (struct file_stream *)stream;
	return fclose(file_stream->f);
}

int file_stream_init(struct file_stream *stream, char *filename, const char *mode) {
	stream->f = fopen(filename, mode);
	stream->stream._errno = errno;
	if(!stream->f) return -1;

	stream->stream.read = file_stream_read;
	stream->stream.write = file_stream_write;
	stream->stream.seek = file_stream_seek;
	stream->stream.eof = file_stream_eof;
	stream->stream.tell = file_stream_tell;
	stream->stream.vprintf = file_stream_vprintf;
	stream->stream.get_memory_access = file_stream_get_memory_access;
	stream->stream.revoke_memory_access = file_stream_revoke_memory_access;
	stream->stream.close = file_stream_close;

	return 0;
}

struct stream *file_stream_create(char *filename, const char *mode) {
	struct file_stream *s = malloc(sizeof(struct file_stream));
	if(!s) return 0;
	int r = file_stream_init(s, filename, mode);
	if(r) {
		free(s);
		return 0;
	}
	return &s.stream;
}

#ifdef HAVE_LIBZIP
static size_t zip_file_stream_read(struct stream *, void *ptr, size_t size) {
	struct zip_file_stream *zip_file_stream = (struct zip_file_stream *)stream;
	size_t r = zip_fread(zip_file_stream->f, ptr, size);
	stream->_errno = zip_error_code_system(zip_file_get_error(zip_file_stream->f));
	return r;
}

static size_t zip_file_stream_write(struct stream *stream, void *ptr, size_t size) {
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
	int t = zip_ftell(zip_file_stream->f);
	if(t < 0) return t;
	return t == stream->stat.size;
}

static long zip_file_stream_tell(struct stream *) {
	struct zip_file_stream *zip_file_stream = (struct zip_file_stream *)stream;
	return zip_ftell(zip_file_stream->f);
}

static int zip_file_stream_vprintf(struct stream *, const char *fmt, va_list ap) {
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

static void zip_file_stream_revoke_memory_access(struct stream *stream) {
	if(stream->mem) free(stream->mem);
}

static int zip_file_stream_close(struct stream *stream) {
	struct zip_file_stream *zip_file_stream = (struct zip_file_stream *)stream;
	int r = zip_fclose(zip_file_stream->f);
	stream->_errno = r;
	return r;
}

int zip_file_stream_init_index(struct zip_file_stream *stream, zip_t *zip, int index)  {
	int r = zip_stat_index(zip, index, ZIP_STAT_SIZE, &stream->stat);
	stream->stream._errno = zip_error_code_system(zip_get_error(zip));
	if(r) return r;

	stream->f = zip_fopen_index(zip, index, 0);
	stream->stream._errno = zip_error_code_system(zip_get_error(zip));
	if(!stream->f) return -1;

	stream->stream.read = zip_file_stream_read;
	stream->stream.write = zip_file_stream_write;
	stream->stream.seek = zip_file_stream_seek;
	stream->stream.eof = zip_file_stream_eof;
	stream->stream.tell = zip_file_stream_tell;
	stream->stream.vprintf = zip_file_stream_vprintf;
	stream->stream.get_memory_access = zip_file_stream_get_memory_access;
	stream->stream.revoke_memory_access = zip_file_stream_revoke_memory_access;
	stream->stream.close = zip_file_stream_close;

	return 0;
}

struct stream *zip_file_stream_create_index(zip_t *zip, int index) {
	struct zip_file_stream *s = malloc(sizeof(struct zip_file_stream));
	if(!s) return 0;
	int r = zip_file_stream_init(s, filename, mode);
	if(r) {
		free(s);
		return 0;
	}
	return &s.stream;
}
#endif

int each_file(const char *path, int (*process_file)(const char *, void *), void *data_ptr) {
	struct stat st;
	int r = stat(path, &st);
	if(r < 0) {
		fprintf(stderr, "Could not stat %s: %s (%d)\n", path, strerror(errno), errno);
		return errno;
	}
	if(S_ISDIR(st.st_mode)) {
		DIR *d = opendir(path);
		if(!d) {
			fprintf(stderr, "Could not opendir %s: %s (%d)\n", path, strerror(errno), errno);
			return errno;
		}
		struct dirent *de;
		while((de = readdir(d))) {
			if(de->d_type != DT_REG && de->d_type != DT_DIR) continue;
			if(de->d_name[0] == '.' && de->d_name[1] == 0) continue;
			if(de->d_name[0] == '.' && de->d_name[1] == '.' && de->d_name[2] == 0) continue;
			char rpath[PATH_MAX]; // FIXME: maybe allocate new string instead?
			snprintf(rpath, sizeof(rpath), "%s/%s", path, de->d_name);
			each_file(rpath, process_file, data_ptr);
		}
		if(closedir(d)) {
			fprintf(stderr, "Could not closedir %s: %s (%d)\n", path, strerror(errno), errno);
			return errno;
		}
	} else if(S_ISREG(st.st_mode)) {
		int r = process_file(path, data_ptr);
		if(r) return r;
	}
	return 0;
}
