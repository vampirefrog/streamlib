#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <dirent.h>
#include <sys/stat.h>
#ifdef __APPLE__
#include <sys/syslimits.h>
#endif
#ifdef WIN32
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#endif

#include "each_file.h"

const char *each_file_strerror(int err) {
	switch (err) {
		case EACHFILE_OK: return "No error";
		case EACHFILE_ERR_OPEN: return "Failed to open directory or file";
		case EACHFILE_ERR_STAT: return "Failed to stat file";
		case EACHFILE_ERR_READDIR: return "Failed to read directory";
		case EACHFILE_ERR_CLOSE: return "Failed to close directory";
		case EACHFILE_ERR_UNKNOWN: return "Unknown each_file error";
		default:
			return strerror(errno);
	}
}

static int each_file_dir(const char *path, struct file_type_filter *filters, int flags) {
	DIR *d = opendir(path);
	if(!d) return EACHFILE_ERR_OPEN;
	struct dirent *de;
	while((de = readdir(d))) {
		if(de->d_name[0] == '.' && de->d_name[1] == 0) continue;
		if(de->d_name[0] == '.' && de->d_name[1] == '.' && de->d_name[2] == 0) continue;
		char rpath[PATH_MAX + 1 + sizeof(de->d_name) + 1]; // FIXME: maybe allocate new string instead?
		if(path[0])
			snprintf(rpath, sizeof(rpath) / sizeof(rpath[0]), "%s/%s", path, de->d_name);
		else
			snprintf(rpath, sizeof(rpath) / sizeof(rpath[0]), "%s", de->d_name);
		each_file(rpath, filters, flags);
	}
	if(closedir(d)) return EACHFILE_ERR_CLOSE;
	return EACHFILE_OK;
}

#ifdef WIN32
static int each_file_dirw(const wchar_t *path, struct file_type_filterw *filters, int flags) {
	WIN32_FIND_DATAW fdata;
	HANDLE h = FindFirstFileW(path, &fdata);
	if(h == INVALID_HANDLE_VALUE) return -1;
	do {
		wchar_t rpath[MAX_PATH + 2];
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
	if(!z) return EACHFILE_ERR_OPEN;
	int num_entries = zip_get_num_entries(z, 0);
	if(num_entries < 0) {
		zip_close(z);
		return EACHFILE_ERR_READDIR;
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
#ifdef HAVE_GZIP
			int r = zip_file_stream_init_index(&s, z, j, (flags & EF_TRANSPARENT_GZIP) ? STREAM_TRANSPARENT_GZIP : 0);
#else
			int r = zip_file_stream_init_index(&s, z, j, 0);
#endif
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
#ifdef HAVE_GZIP
			int r = zip_file_stream_init_index(&s, z, j, (flags & EF_TRANSPARENT_GZIP) ? STREAM_TRANSPARENT_GZIP : 0);
#else
			int r = zip_file_stream_init_index(&s, z, j, 0);
#endif
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
#ifdef HAVE_LIBZIP
		p.zip_file_name = p.zip_file_base = p.zip_file_dirname = 0;
#endif
		if(flags & EF_OPEN_STREAM) {
			struct file_stream s;
#ifdef HAVE_GZIP
			int r = file_stream_init(&s, path, "rb", (flags & EF_TRANSPARENT_GZIP) ? STREAM_TRANSPARENT_GZIP : 0);
#else
			int r = file_stream_init(&s, path, "rb", 0);
#endif
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
#ifdef HAVE_GZIP
			int r = file_stream_initw(&s, path, L"rb", (flags & EF_TRANSPARENT_GZIP) ? STREAM_TRANSPARENT_GZIP : 0);
#else
			int r = file_stream_initw(&s, path, L"rb", 0);
#endif
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
