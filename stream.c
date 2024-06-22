#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <dirent.h>
#include <sys/stat.h>
#ifdef HAVE_LIBZIP
#include <zip.h>
#endif
#ifdef WIN32
#include <windows.h>
#include <fcntl.h>
#else
#include <sys/mman.h>
#endif
#include "stream.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

void stream_init(struct stream *stream) {
	memset(stream, 0, sizeof(*stream));
}

size_t stream_read(struct stream *stream, void *ptr, size_t size) {
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

int stream_read_compare(struct stream *stream, const void *data, size_t len) {
	if(!len) len = strlen((char *)data);
	void *buf = malloc(len);
	if(!buf) return 0;
	int ret = !(stream_read(stream, buf, len) < len || memcmp(buf, data, len));
	free(buf);
	return ret;
}

static size_t mem_stream_read(struct stream *stream, void *ptr, size_t size) {
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

static size_t mem_stream_write(struct stream *stream, const void *ptr, size_t size) {
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

int mem_stream_init(struct mem_stream *stream, void *existing_data, size_t existing_data_len) {
	stream_init(&stream->stream);

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

struct stream *mem_stream_new(void *existing_data, size_t existing_data_len) {
	struct mem_stream *s = malloc(sizeof(struct mem_stream));
	if(!s) return 0;
	int r = mem_stream_init(s, existing_data, existing_data_len);
	if(r) {
		free(s);
		return 0;
	}
	return &s->stream;
}

static size_t file_stream_read(struct stream *stream, void *ptr, size_t size) {
	struct file_stream *file_stream = (struct file_stream *)stream;
	size_t r = fread(ptr, 1, size, file_stream->f);
	stream->_errno = errno;
	return r;
}

static size_t file_stream_write(struct stream *stream, const void *ptr, size_t size) {
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
	return vfprintf(file_stream->f, fmt, ap);
}

static void *file_stream_get_memory_access(struct stream *stream, size_t *length) {
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
	return fclose(file_stream->f);
}

static int file_stream_init_callbacks(struct stream *stream) {
	stream->read = file_stream_read;
	stream->write = file_stream_write;
	stream->seek = file_stream_seek;
	stream->eof = file_stream_eof;
	stream->tell = file_stream_tell;
	stream->vprintf = file_stream_vprintf;
	stream->get_memory_access = file_stream_get_memory_access;
	stream->revoke_memory_access = file_stream_revoke_memory_access;
	stream->close = file_stream_close;
	return 0;
}

int file_stream_init(struct file_stream *stream, const char *filename, const char *mode) {
	stream_init(&stream->stream);

	stream->f = fopen(filename, mode);
	stream->stream._errno = errno;
	if(!stream->f) return -1;

	return file_stream_init_callbacks(&stream->stream);
}

#ifdef WIN32
int file_stream_initw(struct file_stream *stream, const wchar_t *filename, const wchar_t *mode) {
	stream->f = _wfopen(filename, mode);
	stream->stream._errno = errno;
	if(!stream->f) return -1;

	return file_stream_init_callbacks(&stream->stream);
}
#endif

struct stream *file_stream_new(const char *filename, const char *mode) {
	struct file_stream *s = malloc(sizeof(struct file_stream));
	if(!s) return 0;
	int r = file_stream_init(s, filename, mode);
	if(r) {
		free(s);
		return 0;
	}
	return &s->stream;
}

#ifdef WIN32
struct stream *file_stream_neww(const wchar_t *filename, const wchar_t *mode) {
	struct file_stream *s = malloc(sizeof(struct file_stream));
	if(!s) return 0;
	int r = file_stream_initw(s, filename, mode);
	if(r) {
		free(s);
		return 0;
	}
	return &s->stream;
}
#endif

#ifdef HAVE_LIBZIP
static size_t zip_file_stream_read(struct stream *stream, void *ptr, size_t size) {
	struct zip_file_stream *zip_file_stream = (struct zip_file_stream *)stream;
	size_t r = zip_fread(zip_file_stream->f, ptr, size);
	stream->_errno = zip_error_code_system(zip_file_get_error(zip_file_stream->f));
	return r;
}

static size_t zip_file_stream_write(struct stream *stream, const void *ptr, size_t size) {
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

int zip_file_stream_init_index(struct zip_file_stream *stream, zip_t *zip, int index)  {
	stream_init(&stream->stream);

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
	int r = zip_file_stream_init_index(s, zip, index);
	if(r) {
		free(s);
		return 0;
	}
	return (struct stream *)s;
}
#endif

static int each_file_dir(const char *path, struct file_type_filter *filters, int flags) {
	DIR *d = opendir(path);
	if(!d) return errno;
	struct dirent *de;
	while((de = readdir(d))) {
		if(de->d_name[0] == '.' && de->d_name[1] == 0) continue;
		if(de->d_name[0] == '.' && de->d_name[1] == '.' && de->d_name[2] == 0) continue;
		char rpath[PATH_MAX]; // FIXME: maybe allocate new string instead?
		if(path[0])
			snprintf(rpath, sizeof(rpath), "%s/%s", path, de->d_name);
		else
			snprintf(rpath, sizeof(rpath), "%s", de->d_name);
		each_file(rpath, filters, flags);
	}
	if(closedir(d)) return errno;
		return 0;
}

#ifdef WIN32
static int each_file_dirw(const wchar_t *path, struct file_type_filterw *filters, int flags) {
	WIN32_FIND_DATAW fdata;
	HANDLE h = FindFirstFileW(path, &fdata);
	if(h == INVALID_HANDLE_VALUE) return -1;
	do {
		wchar_t rpath[MAX_PATH];
		if(path[0])
			swprintf(rpath, MAX_PATH, L"%s\\%s", path, fdata.cFileName);
		else
			swprintf(rpath, MAX_PATH, L"%s", fdata.cFileName);
		int r = each_filew(rpath, filters, flags);
		if(r) {
			FindClose(h);
			return r;
		}
	} while (FindNextFileW(h, &fdata) != 0);
	FindClose(h);
	return 0;
}
#endif

#define FILL_PATH_INFO(path) \
	p.file_name = (char *)path; \
	char *file_dirname_str = strdup(path); \
	p.file_dirname = dirname(file_dirname_str); \
	char *file_basename_str = strdup(path); \
	p.file_basename = basename(file_basename_str); \
	char *file_ext_str = strdup(p.file_basename); \
	char *file_ext = strrchr(file_ext_str, '.'); \
	if(file_ext) { \
		*file_ext = 0; \
		p.file_ext = file_ext[1] ? file_ext + 1 : 0; \
	} \
	p.file_base = file_ext_str;

#define FILL_PATH_INFOW(path) \
	p.file_name = (wchar_t *)path; \
	wchar_t *file_dirname_str = _wcsdup(path); \
	p.file_dirname = _wdirname(file_dirname_str); \
	wchar_t *file_basename_str = _wcsdup(path); \
	p.file_basename = _wbasename(file_basename_str); \
	wchar_t *file_ext_str = _wcsdup(p.file_basename); \
	wchar_t *file_ext = wcsrchr(file_ext_str, '.'); \
	if(file_ext) { \
		*file_ext = 0; \
		p.file_ext = file_ext[1] ? file_ext + 1 : 0; \
	} \
	p.file_base = file_ext_str;

#define FREE_PATH_INFO() \
	free(file_ext_str); \
	free(file_basename_str); \
	free(file_dirname_str);

#ifdef HAVE_LIBZIP
static int each_file_zip(const char *path, struct file_type_filter *filters, int flags) {
	(void)flags;
	int err;
	zip_t *z = zip_open(path, ZIP_RDONLY, &err);
	if(!z) return err;
	int num_entries = zip_get_num_entries(z, 0);
	if(num_entries < 0) {
		zip_close(z);
		return -1;
	}
	struct path_info p;
	p.zip_file_name = (char *)path;
	char *zip_dirname_str = strdup(path);
	p.zip_file_dirname = dirname(zip_dirname_str);
	char *zip_basename_str = strdup(path);
	p.zip_file_base = basename(zip_basename_str);
	char *zip_ext = strrchr(p.zip_file_base, '.');
	if(zip_ext) *zip_ext = 0;

	for(int j = 0; j < num_entries; j++) {
		zip_stat_t st;
		zip_stat_index(z, j, ZIP_STAT_NAME | ZIP_STAT_SIZE, &st);
		const char *ext = strrchr(st.name, '.');
		if(!ext || !ext[1]) continue;
		for(struct file_type_filter *f = filters; f->ext; f++) {
			if(strcasecmp(ext, f->ext)) continue;
			struct zip_file_stream s;
			int r = zip_file_stream_init_index(&s, z, j);
			if(r) return r;
			FILL_PATH_INFO(st.name);
			r = f->file_cb(&p, (struct stream *)&s, f->user_data);
			FREE_PATH_INFO();
			stream_close((struct stream *)&s);
			if(r) return r;
			break;
		}
	}
	free(zip_basename_str);
	free(zip_dirname_str);
	zip_close(z);
	return 0;
}

#ifdef WIN32
wchar_t *_wbasename(wchar_t* path) {
	wchar_t* base = wcspbrk(path, L"\\/");
	return base ? base + 1 : path;
}

wchar_t* _wdirname(wchar_t* path) {
	wchar_t* last_backslash = wcspbrk(path, L"\\/");

	if(last_backslash != NULL) {
		if(last_backslash == path)
			*(last_backslash + 1) = L'\0';
		else
			*last_backslash = L'\0';
	} else wcscpy(path, L".");

	return path;
}

static int each_file_zipw(const wchar_t *path, struct file_type_filterw *filters, int flags) {
	(void)flags;
	int fd = _wopen(path, _O_RDONLY | _O_BINARY);
	if(!fd) return errno;
	int err;
	zip_t *z = zip_fdopen(fd, 0, &err);
	if(!z) return err;
	int num_entries = zip_get_num_entries(z, 0);
	if(num_entries < 0) {
		zip_close(z);
		return -1;
	}
	struct path_infow p;
	p.zip_file_name = (wchar_t *)path;
	wchar_t *zip_dirname_str = _wcsdup(path);
	p.zip_file_dirname = _wdirname(zip_dirname_str);
	wchar_t *zip_basename_str = _wcsdup(path);
	p.zip_file_base = _wbasename(zip_basename_str);
	wchar_t *zip_ext = wcsrchr(p.zip_file_base, L'.');
	if(zip_ext) *zip_ext = 0;

	for(int j = 0; j < num_entries; j++) {
		zip_stat_t st;
		zip_stat_index(z, j, ZIP_STAT_NAME | ZIP_STAT_SIZE, &st);
		int l = strlen(st.name);
		wchar_t stname[MAX_PATH];
		if(l >= MAX_PATH) l = MAX_PATH-1;
		for(int k = 0; k < l; k++)
			stname[k] = st.name[k];
		const wchar_t *ext = wcsrchr(stname, L'.');
		if(!ext || !ext[1]) continue;
		for(struct file_type_filterw *f = filters; f->ext; f++) {
			if(_wcsicmp(ext, f->ext)) continue;
			struct zip_file_stream s;
			int r = zip_file_stream_init_index(&s, z, j);
			if(r) return r;
			FILL_PATH_INFOW(stname);
			r = f->file_cb(&p, (struct stream *)&s, f->user_data);
			FREE_PATH_INFO();
			stream_close((struct stream *)&s);
			if(r) return r;
			break;
		}
	}
	free(zip_basename_str);
	free(zip_dirname_str);
	zip_close(z);
	return 0;
}
#endif /* WIN32 */

#endif /* HAVE_LIBZIP */

static int each_file_file(const char *path, const char *ext, struct file_type_filter *filters, int flags) {
	for(struct file_type_filter *f = filters; f->ext; f++) {
		if(strcasecmp(ext, f->ext)) continue;
		struct path_info p;
		p.zip_file_name = p.zip_file_base = p.zip_file_dirname = 0;
		if(flags & EF_OPEN_STREAM) {
			struct file_stream s;
			int r = file_stream_init(&s, path, "rb");
			if(r) return r;
			FILL_PATH_INFO(path);
			r = f->file_cb(&p, (struct stream *)&s, f->user_data);
			FREE_PATH_INFO();
			stream_close((struct stream *)&s);
			if(r) return r;
		} else {
			FILL_PATH_INFO(path);
			f->file_cb(&p, 0, f->user_data);
			FREE_PATH_INFO();
		}
		return 0;
	}
	return 1;
}

#ifdef WIN32
static int each_file_filew(const wchar_t *path, const wchar_t *ext, struct file_type_filterw *filters, int flags) {
	for(struct file_type_filterw *f = filters; f->ext; f++) {
		if(_wcsicmp(ext, f->ext)) continue;
		struct path_infow p;
		p.zip_file_name = p.zip_file_base = p.zip_file_dirname = 0;
		if(flags & EF_OPEN_STREAM) {
			struct file_stream s;
			int r = file_stream_initw(&s, path, L"rb");
			if(r) return r;
			FILL_PATH_INFOW(path);
			r = f->file_cb(&p, (struct stream *)&s, f->user_data);
			FREE_PATH_INFO();
			stream_close((struct stream *)&s);
			if(r) return r;
		} else {
			FILL_PATH_INFOW(path);
			f->file_cb(&p, 0, f->user_data);
			FREE_PATH_INFO();
		}
		return 0;
	}
	return 1;
}
#endif

int each_file(const char *path, struct file_type_filter *filters, int flags) {
	struct stat st;
	int r = stat(path, &st);
	if(r < 0) return errno;
	if(S_ISDIR(st.st_mode) && (flags & EF_RECURSE_DIRS)) {
		return each_file_dir(path, filters, flags);
	} else {
		const char *ext = strrchr(path, '.');
		if(ext) {
#ifdef HAVE_LIBZIP
			if(!strcasecmp(ext, ".zip") && (flags & EF_RECURSE_ARCHIVES) && (flags & EF_OPEN_STREAM))
				return each_file_zip(path, filters, flags);
#endif
			return each_file_file(path, ext, filters, flags);
		}
	}
	return 0;
}

#ifdef WIN32
int each_filew(const wchar_t *path, struct file_type_filterw *filters, int flags) {
	struct _stat st;
	int r = _wstat(path, &st);
	if(r < 0) return errno;
	if(S_ISDIR(st.st_mode) && (flags & EF_RECURSE_DIRS)) {
		return each_file_dirw(path, filters, flags);
	} else {
		const wchar_t *ext = wcsrchr(path, L'.');
		if(ext) {
#ifdef HAVE_LIBZIP
			if(!_wcsicmp(ext, L".zip") && (flags & EF_RECURSE_ARCHIVES) && (flags & EF_OPEN_STREAM))
				return each_file_zipw(path, filters, flags);
#endif
			return each_file_filew(path, ext, filters, flags);
		}
	}
	return 0;
}
#endif
