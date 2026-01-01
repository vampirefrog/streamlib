/**
 * @file walker.c
 * @brief Path walker implementation
 */

#include "stream.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <dirent.h>
#include <sys/stat.h>

/* Forward declarations */
static int walk_directory(const char *path, walker_fn callback, void *userdata,
			  unsigned int flags, int depth);
static int walk_file(const char *path, walker_fn callback, void *userdata,
		     unsigned int flags, int depth);

#ifdef STREAM_HAVE_LIBARCHIVE
#include <archive.h>
#include <archive_entry.h>

static int walk_archive(const char *base_path, struct stream *stream,
			walker_fn callback, void *userdata,
			unsigned int flags, int depth);

/* Check if a filename looks like an archive */
static int is_archive_filename(const char *path)
{
	const char *ext = strrchr(path, '.');
	if (!ext)
		return 0;

	/* Common archive extensions */
	if (strcmp(ext, ".tar") == 0 ||
	    strcmp(ext, ".zip") == 0 ||
	    strcmp(ext, ".7z") == 0 ||
	    strcmp(ext, ".rar") == 0 ||
	    strcmp(ext, ".gz") == 0 ||   /* Could be .tar.gz */
	    strcmp(ext, ".bz2") == 0 ||  /* Could be .tar.bz2 */
	    strcmp(ext, ".xz") == 0 ||   /* Could be .tar.xz */
	    strcmp(ext, ".zst") == 0)    /* Could be .tar.zst */
		return 1;

	return 0;
}

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
#ifdef STREAM_HAVE_ZLIB
	struct compression_stream cs;
#endif

	if (!entry.is_dir) {
		if (file_stream_open(&fs, path, O_RDONLY, 0) == 0) {
			file_stream_ptr = &fs.base;

#ifdef STREAM_HAVE_ZLIB
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

#ifdef STREAM_HAVE_LIBARCHIVE
	/* Check if we should expand archives */
	if ((flags & WALK_EXPAND_ARCHIVES) && !entry.is_dir &&
	    is_archive_filename(path)) {
		/* Open file */
		struct file_stream fs;
		if (file_stream_open(&fs, path, O_RDONLY, 0) < 0)
			return 0;  /* Skip on error */

		/* Try to open as archive */
		ret = walk_archive(path, &fs.base, callback, userdata,
				   flags, depth + 1);
		stream_close(&fs.base);

		if (ret < 0 && ret != -EIO)
			return ret;  /* Propagate errors except I/O errors */
	}
#endif

	return 0;
}

/* Walk a directory recursively */
static int walk_directory(const char *path, walker_fn callback, void *userdata,
			  unsigned int flags, int depth)
{
	DIR *dir = opendir(path);
	if (!dir)
		return -errno;

	struct stat st;
	if (stat(path, &st) < 0) {
		closedir(dir);
		return -errno;
	}

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
			closedir(dir);
			return ret;
		}
	}

	/* If not recursing, stop here */
	if (!(flags & WALK_RECURSE_DIRS)) {
		closedir(dir);
		return 0;
	}

	/* Iterate through directory entries */
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
	return 0;
}

#ifdef STREAM_HAVE_LIBARCHIVE
/* Walk archive entries */
static int walk_archive(const char *base_path, struct stream *stream,
			walker_fn callback, void *userdata,
			unsigned int flags, int depth)
{
	struct archive_stream ar;
	int ret;

	/* Try to decompress if it's a compressed archive */
#ifdef STREAM_HAVE_ZLIB
	struct compression_stream cs;
	struct stream *original_stream = stream;

	if (flags & WALK_DECOMPRESS) {
		stream = stream_auto_decompress(stream, &cs, 0);
	}
#endif

	/* Open as archive */
	ret = archive_stream_open(&ar, stream, 0);
	if (ret < 0) {
#ifdef STREAM_HAVE_ZLIB
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
#ifdef STREAM_HAVE_ZLIB
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
#ifdef STREAM_HAVE_ZLIB
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

#ifdef STREAM_HAVE_ZLIB
	/* Close compression stream if we created one */
	if ((flags & WALK_DECOMPRESS) && stream != original_stream)
		stream_close(stream);
#endif

	return 0;
}
#endif /* STREAM_HAVE_LIBARCHIVE */

/* Main walk_path function */
int walk_path(const char *path, walker_fn callback, void *userdata,
	      unsigned int flags)
{
	struct stat st;

	/* Check for unsupported features */
#ifndef STREAM_HAVE_LIBARCHIVE
	if (flags & WALK_EXPAND_ARCHIVES)
		return -ENOSYS;
#endif

#ifndef STREAM_HAVE_ZLIB
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
