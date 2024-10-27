#include <errno.h>
#include <sys/stat.h>
#include <stdlib.h>
#ifdef WIN32
#include <windows.h>
#include <fcntl.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#endif
#ifdef HAVE_GZIP
#include <zlib.h>
#endif

#include "file_stream.h"

static ssize_t file_stream_read(struct stream *stream, void *ptr, size_t size) {
	struct file_stream *file_stream = (struct file_stream *)stream;
	ssize_t r = fread(ptr, 1, size, file_stream->f);
	stream->_errno = errno;
	return r;
}

static ssize_t file_stream_write(struct stream *stream, const void *ptr, size_t size) {
	struct file_stream *file_stream = (struct file_stream *)stream;
	ssize_t r = fwrite(ptr, 1, size, file_stream->f);
	stream->_errno = errno;
	return r;
}

static size_t file_stream_seek(struct stream *stream, long offset, int whence) {
	struct file_stream *file_stream = (struct file_stream *)stream;
	off_t r = fseek(file_stream->f, offset, whence);
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
	long l = ftell(file_stream->f);
	stream->_errno = errno;
	return l;
}

static int file_stream_vprintf(struct stream *stream, const char *fmt, va_list ap) {
	struct file_stream *file_stream = (struct file_stream *)stream;
	return vfprintf(file_stream->f, fmt, ap);
}

static void *file_stream_get_memory_access(struct stream *stream, size_t *length) {
	struct file_stream *file_stream = (struct file_stream *)stream;

	int fd = fileno(file_stream->f);

	struct stat st;
	if(fstat(fd, &st) == -1) {
		stream->_errno = errno;
		return 0;
	}
	if(length) *length = st.st_size;

#ifdef WIN32
	HANDLE fileMapping = CreateFileMapping((HANDLE)_get_osfhandle(fd), NULL, PAGE_READONLY, 0, 0, NULL);
	if(!fileMapping) return 0;

	stream->mem = MapViewOfFile(fileMapping, FILE_MAP_READ, 0, 0, st.st_size);

	CloseHandle(fileMapping);

	return stream->mem;
#else
	// Map file into memory using mmap
	stream->mem_size = st.st_size;
	stream->mem = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if(stream->mem == MAP_FAILED) {
		stream->_errno = errno;
		return 0;
	}

	return stream->mem;
#endif
}

static int file_stream_revoke_memory_access(struct stream *stream) {
#ifdef WIN32
	return UnmapViewOfFile(stream->mem) ? 0 : -1;
#else
	return munmap(stream->mem, stream->mem_size);
#endif
}

static int file_stream_close(struct stream *stream) {
	struct file_stream *file_stream = (struct file_stream *)stream;
	int r = fclose(file_stream->f);
	file_stream->f = 0;
	stream->_errno = errno;
	return r;
}

static int file_stream_init_fp(struct file_stream *stream, FILE *f) {
	stream->f = f;
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

#ifdef HAVE_GZIP
static ssize_t file_stream_read_gz(struct stream *stream, void *ptr, size_t size) {
	struct file_stream *file_stream = (struct file_stream *)stream;
	ssize_t r = gzread(file_stream->gz, ptr, size);
	stream->_errno = errno;
	return r;
}

static ssize_t file_stream_write_gz(struct stream *stream, const void *ptr, size_t size) {
	struct file_stream *file_stream = (struct file_stream *)stream;
	ssize_t r = gzwrite(file_stream->gz, ptr, size);
	stream->_errno = errno;
	return r;
}

static size_t file_stream_seek_gz(struct stream *stream, long offset, int whence) {
	struct file_stream *file_stream = (struct file_stream *)stream;
	off_t r = gzseek(file_stream->gz, offset, whence);
	stream->_errno = errno;
	return r;
}

static int file_stream_eof_gz(struct stream *stream) {
	struct file_stream *file_stream = (struct file_stream *)stream;
	int r = gzeof(file_stream->gz);
	stream->_errno = errno;
	return r;
}

static long file_stream_tell_gz(struct stream *stream) {
	struct file_stream *file_stream = (struct file_stream *)stream;
	long l = gztell(file_stream->gz);
	stream->_errno = errno;
	return l;
}

static int file_stream_vprintf_gz(struct stream *stream, const char *fmt, va_list ap) {
	struct file_stream *file_stream = (struct file_stream *)stream;
	return gzvprintf(file_stream->gz, fmt, ap);
}

static void *file_stream_get_memory_access_gz(struct stream *stream, size_t *length) {
	(void)stream;
	(void)length;
	return 0;
}

static int file_stream_revoke_memory_access_gz(struct stream *stream) {
	(void)stream;
	return 0;
}

static int file_stream_close_gz(struct stream *stream) {
	struct file_stream *file_stream = (struct file_stream *)stream;
	int r = gzclose(file_stream->gz);
	file_stream->gz = 0;
	stream->_errno = errno;
	return r;
}

static int file_stream_init_gz(struct file_stream *stream, gzFile gz) {
	stream->gz = gz;
	stream->stream.read = file_stream_read_gz;
	stream->stream.write = file_stream_write_gz;
	stream->stream.seek = file_stream_seek_gz;
	stream->stream.eof = file_stream_eof_gz;
	stream->stream.tell = file_stream_tell_gz;
	stream->stream.vprintf = file_stream_vprintf_gz;
	stream->stream.get_memory_access = file_stream_get_memory_access_gz;
	stream->stream.revoke_memory_access = file_stream_revoke_memory_access_gz;
	stream->stream.close = file_stream_close_gz;
	return 0;
}
#endif

int file_stream_init(struct file_stream *stream, const char *filename, const char *mode, int stream_flags) {
	stream_init(&stream->stream, stream_flags);
#ifdef HAVE_GZIP
	if(stream_flags & STREAM_TRANSPARENT_GZIP) {
		gzFile f = gzopen(filename, mode);
		stream->stream._errno = errno;
		if(!f) return errno;
		return file_stream_init_gz(stream, f);
	}
#endif
	FILE *f = fopen(filename, mode);
	stream->stream._errno = errno;
	if(!f) return errno;
	return file_stream_init_fp(stream, f);
}

struct stream *file_stream_new(const char *filename, const char *mode, int stream_flags) {
	struct file_stream *s = malloc(sizeof(struct file_stream));
	if(!s) return 0;
	int r = file_stream_init(s, filename, mode, stream_flags);
	if(r) {
		free(s);
		return 0;
	}
	return &s->stream;
}

#ifdef WIN32
int file_stream_initw(struct file_stream *stream, const wchar_t *filename, const wchar_t *mode, int stream_flags) {
	stream_init(&stream->stream, stream_flags);
#ifdef HAVE_GZIP
	if(stream_flags & STREAM_TRANSPARENT_GZIP) {
		char cmode[10];
		memset(cmode, 0, sizeof(cmode));
		for(int i = 0; i < strlen(mode) && i < sizeof(cmode) / sizeof(cmode[0]); i++)
			cmode[i] = mode[i];
		gzFile *f = gzopen_w(filename, cmode);
		stream->stream._errno = errno;
		if(!f) return errno;
		return file_stream_init_gz(f);
	}
#endif
	FILE *f = _wfopen(filename, mode);
	if(!f) return errno;
	return file_stream_init_fp(stream, f);
}

struct stream *file_stream_neww(const wchar_t *filename, const wchar_t *mode, int stream_flags) {
	struct file_stream *s = malloc(sizeof(struct file_stream));
	if(!s) return 0;
	int r = file_stream_initw(s, filename, mode, stream_flags);
	if(r) {
		free(s);
		return 0;
	}
	return &s->stream;
}
#endif
