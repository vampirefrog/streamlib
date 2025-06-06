#pragma once

#include "stream_base.h"
#include "file_stream.h"
#include "zip_file_stream.h"

struct path_info {
#ifdef HAVE_LIBZIP
	char *zip_file_name;       // foo/bar/baz.zip
	char *zip_file_dirname;    // foo/bar
	char *zip_file_base;       // baz
#endif

	char *file_name;           // foo/bar/baz.txt
	char *file_dirname;        // foo/bar
	char *file_basename;       // baz.txt
	char *file_base;           // baz
	char *file_ext;            // txt
};
#ifdef WIN32
struct path_infow {
#ifdef HAVE_LIBZIP
	wchar_t *zip_file_name;    // foo/bar/baz.zip
	wchar_t *zip_file_dirname; // foo/bar
	wchar_t *zip_file_base;    // baz
#endif

	wchar_t *file_name;        // foo/bar/baz.txt
	wchar_t *file_dirname;     // foo/bar
	wchar_t *file_basename;    // baz.txt
	wchar_t *file_base;        // baz
	wchar_t *file_ext;         // txt
};
#endif

struct file_type_filter {
	const char *ext;
	int (*file_cb)(struct path_info *path_info, struct stream *stream, void *user_data);
	void *user_data;
};
#ifdef WIN32
struct file_type_filterw {
	const wchar_t *ext;
	int (*file_cb)(struct path_infow *path_info, struct stream *stream, void *user_data);
	void *user_data;
};
#endif

#define EF_RECURSE_DIRS 0x01
#ifdef HAVE_LIBZIP
#define EF_RECURSE_ARCHIVES 0x02
#endif
#define EF_OPEN_STREAM 0x04
#ifdef HAVE_GZIP
#define EF_TRANSPARENT_GZIP 0x08
#endif

#define EACHFILE_OK 0
#define EACHFILE_ERR_OPEN    -1
#define EACHFILE_ERR_STAT    -2
#define EACHFILE_ERR_READDIR -3
#define EACHFILE_ERR_CLOSE   -4
#define EACHFILE_ERR_UNKNOWN -100

const char *each_file_strerror(int err);

int each_file(const char *path, struct file_type_filter *filters, int flags);
#ifdef WIN32
int each_filew(const wchar_t *path, struct file_type_filterw *filters, int flags);
#endif
