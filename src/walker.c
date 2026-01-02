/**
 * @file walker.c
 * @brief Path walker implementation (POSIX and Windows)
 */

#include "stream.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef _WIN32
/* POSIX headers */
#include <libgen.h>
#include <dirent.h>
#include <sys/stat.h>
#else
/* Windows headers */
#include <windows.h>
#include <sys/stat.h>
#define PATH_MAX MAX_PATH

/* basename implementation for Windows */
static const char *basename_win(const char *path) {
	const char *p = strrchr(path, '\\');
	if (!p) p = strrchr(path, '/');
	return p ? p + 1 : path;
}
#define basename(x) basename_win(x)
#endif

/* Forward declarations */
static int walk_directory(const char *path, walker_fn callback, void *userdata,
			  unsigned int flags, int depth);
static int walk_file(const char *path, walker_fn callback, void *userdata,
		     unsigned int flags, int depth);

#ifdef HAVE_LIBARCHIVE
#include <archive.h>
#include <archive_entry.h>

static int walk_archive(const char *base_path, struct stream *stream,
			walker_fn callback, void *userdata,
			unsigned int flags, int depth);

/* Simple stream wrapper for reading from current archive entry */
struct archive_entry_stream {
	struct stream base;
	struct archive *archive;
	off64_t bytes_read;
	off64_t entry_size;
};

static ssize_t archive_entry_stream_read(void *stream_ptr, void *buf, size_t count)
{
	struct archive_entry_stream *s = stream_ptr;

	/* Don't read past end of entry */
	if (s->entry_size >= 0 && s->bytes_read + (off64_t)count > s->entry_size)
		count = s->entry_size - s->bytes_read;

	if (count == 0)
		return 0;

	la_ssize_t nread = archive_read_data(s->archive, buf, count);
	if (nread > 0)
		s->bytes_read += nread;

	return nread;
}

static off64_t archive_entry_stream_tell(void *stream_ptr)
{
	struct archive_entry_stream *s = stream_ptr;
	return s->bytes_read;
}

static off64_t archive_entry_stream_size(void *stream_ptr)
{
	struct archive_entry_stream *s = stream_ptr;
	return s->entry_size;
}

static int archive_entry_stream_close(void *stream_ptr)
{
	/* Don't close the underlying archive, just this wrapper */
	(void)stream_ptr;
	return 0;
}

static const struct stream_ops archive_entry_stream_ops = {
	.read = archive_entry_stream_read,
	.write = NULL,
	.seek = NULL,
	.tell = archive_entry_stream_tell,
	.size = archive_entry_stream_size,
	.mmap = NULL,
	.munmap = NULL,
	.flush = NULL,
	.close = archive_entry_stream_close,
	.vprintf = NULL,
	.get_caps = NULL,
};

static void archive_entry_stream_init(struct archive_entry_stream *s,
				      struct archive *archive,
				      off64_t entry_size)
{
	stream_init(&s->base, &archive_entry_stream_ops, O_RDONLY,
		    STREAM_CAP_READ | STREAM_CAP_TELL | STREAM_CAP_SIZE);
	s->archive = archive;
	s->bytes_read = 0;
	s->entry_size = entry_size;
}

#endif

/* Walk a single file */
static int walk_file(const char *path, walker_fn callback, void *userdata,
		     unsigned int flags, int depth)
{
	struct stat st;

	if (stat(path, &st) < 0)
		return -errno;

	/* Build walker entry */
	struct walker_entry entry;
	entry.path = path;
	entry.name = basename((char *)path);
	entry.size = st.st_size;
	entry.mode = st.st_mode;
	entry.mtime = st.st_mtime;
	entry.is_dir = S_ISDIR(st.st_mode);
	entry.is_archive_entry = 0;
	entry.depth = depth;
	entry.stream = NULL;
	entry.internal_data = NULL;

	/* Apply filters */
	if ((flags & WALK_FILTER_FILES) && entry.is_dir)
		return 0;
	if ((flags & WALK_FILTER_DIRS) && !entry.is_dir)
		return 0;

	/* Open stream for regular files */
	struct file_stream fs;
	struct stream *file_stream_ptr = NULL;
#ifdef HAVE_COMPRESSION
	struct compression_stream cs;
#endif

	if (!entry.is_dir) {
		if (file_stream_open(&fs, path, O_RDONLY, 0) == 0) {
			file_stream_ptr = &fs.base;

#ifdef HAVE_COMPRESSION
			/* Auto-decompress if requested - compression stream owns file stream */
			if (flags & WALK_DECOMPRESS) {
				entry.stream = stream_auto_decompress(file_stream_ptr, &cs, 1);
			} else {
				entry.stream = file_stream_ptr;
			}
#else
			entry.stream = file_stream_ptr;
#endif
		}
	}

	/* Invoke callback */
	int ret = callback(&entry, userdata);

	/* Close stream (closes underlying file stream too if wrapped) */
	if (entry.stream)
		stream_close(entry.stream);

	if (ret != 0)
		return ret;

#ifdef HAVE_LIBARCHIVE
	/* Try to expand archives (detected by magic bytes, not extension!) */
	if ((flags & WALK_EXPAND_ARCHIVES) && !entry.is_dir) {
		/* Open file */
		struct file_stream archive_fs;
		if (file_stream_open(&archive_fs, path, O_RDONLY, 0) < 0)
			return 0;  /* Skip on error */

		/* Try to open as archive - libarchive auto-detects format by magic bytes
		 * Supports: ZIP (50 4B), TAR (ustar at 257), 7z (37 7A BC AF),
		 *           RAR (52 61 72 21), and many more */
		ret = walk_archive(path, &archive_fs.base, callback, userdata,
				   flags, depth + 1);
		stream_close(&archive_fs.base);

		/* If archive walk succeeded or had a non-I/O error, propagate it
		 * I/O errors likely mean "not an archive", so we ignore them */
		if (ret < 0 && ret != -EIO)
			return ret;
	}
#endif

	return 0;
}

/* Walk a directory recursively */
static int walk_directory(const char *path, walker_fn callback, void *userdata,
			  unsigned int flags, int depth)
{
	struct stat st;

#ifndef _WIN32
	/* POSIX implementation */
	DIR *dir = opendir(path);
	if (!dir)
		return -errno;

	if (stat(path, &st) < 0) {
		closedir(dir);
		return -errno;
	}
#else
	/* Windows implementation */
	if (stat(path, &st) < 0)
		return -errno;
#endif

	/* First, callback for the directory itself */
	struct walker_entry entry;
	entry.path = path;
	entry.name = basename((char *)path);
	entry.size = st.st_size;
	entry.mode = st.st_mode;
	entry.mtime = st.st_mtime;
	entry.is_dir = 1;
	entry.is_archive_entry = 0;
	entry.depth = depth;
	entry.stream = NULL;
	entry.internal_data = NULL;

	/* Apply filters */
	if (!(flags & WALK_FILTER_FILES)) {
		int ret = callback(&entry, userdata);
		if (ret != 0) {
#ifndef _WIN32
			closedir(dir);
#endif
			return ret;
		}
	}

	/* If not recursing, stop here */
	if (!(flags & WALK_RECURSE_DIRS)) {
#ifndef _WIN32
		closedir(dir);
#endif
		return 0;
	}

#ifndef _WIN32
	/* POSIX directory iteration */
	struct dirent *ent;
	while ((ent = readdir(dir)) != NULL) {
		/* Skip . and .. */
		if (strcmp(ent->d_name, ".") == 0 ||
		    strcmp(ent->d_name, "..") == 0)
			continue;

		/* Build full path */
		char full_path[PATH_MAX];
		snprintf(full_path, sizeof(full_path), "%s/%s", path, ent->d_name);

		/* Get entry info */
		if (stat(full_path, &st) < 0)
			continue;

		/* Handle symlinks */
		if (S_ISLNK(st.st_mode) && !(flags & WALK_FOLLOW_SYMLINKS))
			continue;

		/* Recurse into subdirectories */
		if (S_ISDIR(st.st_mode)) {
			int ret = walk_directory(full_path, callback, userdata,
						 flags, depth + 1);
			if (ret != 0) {
				closedir(dir);
				return ret;
			}
		} else {
			/* Regular file */
			int ret = walk_file(full_path, callback, userdata,
					    flags, depth + 1);
			if (ret != 0) {
				closedir(dir);
				return ret;
			}
		}
	}

	closedir(dir);
#else
	/* Windows directory iteration */
	char search_path[PATH_MAX];
	snprintf(search_path, sizeof(search_path), "%s\\*", path);

	WIN32_FIND_DATAA find_data;
	HANDLE hFind = FindFirstFileA(search_path, &find_data);

	if (hFind == INVALID_HANDLE_VALUE)
		return -EIO;

	do {
		/* Skip . and .. */
		if (strcmp(find_data.cFileName, ".") == 0 ||
		    strcmp(find_data.cFileName, "..") == 0)
			continue;

		/* Build full path */
		char full_path[PATH_MAX];
		snprintf(full_path, sizeof(full_path), "%s\\%s", path, find_data.cFileName);

		/* Get entry info */
		if (stat(full_path, &st) < 0)
			continue;

		/* Recurse into subdirectories */
		if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			int ret = walk_directory(full_path, callback, userdata,
						 flags, depth + 1);
			if (ret != 0) {
				FindClose(hFind);
				return ret;
			}
		} else {
			/* Regular file */
			int ret = walk_file(full_path, callback, userdata,
					    flags, depth + 1);
			if (ret != 0) {
				FindClose(hFind);
				return ret;
			}
		}
	} while (FindNextFileA(hFind, &find_data));

	FindClose(hFind);
#endif

	return 0;
}

#ifdef HAVE_LIBARCHIVE
/* Walk archive entries */
static int walk_archive(const char *base_path, struct stream *stream,
			walker_fn callback, void *userdata,
			unsigned int flags, int depth)
{
	struct archive_stream ar;
	int ret;

	/* Try to decompress if it's a compressed archive */
#ifdef HAVE_COMPRESSION
	struct compression_stream cs;
	struct stream *original_stream = stream;

	if (flags & WALK_DECOMPRESS) {
		stream = stream_auto_decompress(stream, &cs, 0);
	}
#endif

	/* Open as archive */
	ret = archive_stream_open(&ar, stream, 0);
	if (ret < 0) {
#ifdef HAVE_COMPRESSION
		/* Close compression stream if we created one */
		if ((flags & WALK_DECOMPRESS) && stream != original_stream)
			stream_close(stream);
#endif
		return ret;
	}

	/* Walk archive entries */
	struct archive *a = ar.archive;
	struct archive_entry *ae;

	while (archive_read_next_header(a, &ae) == ARCHIVE_OK) {
		const char *pathname = archive_entry_pathname(ae);

		/* Build full virtual path */
		char full_path[PATH_MAX];
		snprintf(full_path, sizeof(full_path), "%s:%s",
			 base_path, pathname);

		/* Build walker entry */
		struct walker_entry entry;
		entry.path = full_path;
		entry.name = basename((char *)pathname);
		entry.size = archive_entry_size(ae);
		entry.mode = archive_entry_mode(ae);
		entry.mtime = archive_entry_mtime(ae);
		entry.is_dir = S_ISDIR(entry.mode);
		entry.is_archive_entry = 1;
		entry.depth = depth;
		entry.stream = NULL;
		entry.internal_data = NULL;

		/* Create stream for regular files */
		struct archive_entry_stream aes;
		struct compression_stream entry_cs;

		if (!entry.is_dir) {
			archive_entry_stream_init(&aes, a, entry.size);

			/* Auto-decompress if requested - stream library handles it! */
#ifdef HAVE_COMPRESSION
			if (flags & WALK_DECOMPRESS) {
				entry.stream = stream_auto_decompress(&aes.base, &entry_cs, 0);
			} else {
				entry.stream = &aes.base;
			}
#else
			entry.stream = &aes.base;
#endif
		}

		/* Apply filters */
		if ((flags & WALK_FILTER_FILES) && entry.is_dir)
			goto skip;
		if ((flags & WALK_FILTER_DIRS) && !entry.is_dir)
			goto skip;

		/* Invoke callback */
		ret = callback(&entry, userdata);

		if (ret != 0) {
			/* Close entry stream before exiting */
			if (entry.stream)
				stream_close(entry.stream);
			archive_stream_close(&ar);
#ifdef HAVE_COMPRESSION
			/* Close archive compression stream if we created one */
			if ((flags & WALK_DECOMPRESS) && stream != original_stream)
				stream_close(stream);
#endif
			return ret;
		}

skip:
		/* Close entry stream before next iteration */
		if (entry.stream)
			stream_close(entry.stream);

		/* Skip any remaining unread data */
		archive_read_data_skip(a);
	}

	archive_stream_close(&ar);

#ifdef HAVE_COMPRESSION
	/* Close compression stream if we created one */
	if ((flags & WALK_DECOMPRESS) && stream != original_stream)
		stream_close(stream);
#endif

	return 0;
}
#endif /* HAVE_LIBARCHIVE */

/* Main walk_path function */
int walk_path(const char *path, walker_fn callback, void *userdata,
	      unsigned int flags)
{
	struct stat st;

	/* Check for unsupported features */
#ifndef HAVE_LIBARCHIVE
	if (flags & WALK_EXPAND_ARCHIVES)
		return -ENOSYS;
#endif

#ifndef HAVE_COMPRESSION
	if (flags & WALK_DECOMPRESS)
		return -ENOSYS;
#endif

	if (stat(path, &st) < 0)
		return -errno;

	/* Handle based on type */
	if (S_ISDIR(st.st_mode)) {
		return walk_directory(path, callback, userdata, flags, 0);
	} else {
		return walk_file(path, callback, userdata, flags, 0);
	}
}
