/**
 * @file archive_stream.c
 * @brief Archive stream implementation using libarchive
 */

#include "stream.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#ifndef _WIN32
#include <libgen.h>
#else
/* Windows basename implementation */
static const char *basename_win(const char *path) {
	const char *p = strrchr(path, '\\');
	if (!p) p = strrchr(path, '/');
	return p ? p + 1 : path;
}
#define basename(x) basename_win(x)
#endif

#ifdef HAVE_LIBARCHIVE
#include <archive.h>
#include <archive_entry.h>

/* Callback functions for libarchive to read from our stream */
static int archive_stream_close_callback(struct archive *a, void *client_data);
static la_ssize_t archive_stream_read_callback(struct archive *a, void *client_data,
					       const void **buffer);

/* Callback functions for libarchive to write to our stream */
static int stream_archive_write_open(struct archive *a, void *client_data);
static la_ssize_t stream_archive_write_data(struct archive *a, void *client_data,
					     const void *buffer, size_t length);
static int stream_archive_write_close(struct archive *a, void *client_data);

/* Internal buffer for reading from underlying stream */
#define ARCHIVE_BUFFER_SIZE 16384

struct archive_read_data {
	struct stream *underlying;
	unsigned char buffer[ARCHIVE_BUFFER_SIZE];
	int error;
};

/* Open archive from stream */
int archive_stream_open(struct archive_stream *stream,
			struct stream *underlying,
			int owns_underlying)
{
	memset(stream, 0, sizeof(*stream));

	/* Archives are read-only for now */
	unsigned int caps = STREAM_CAP_READ;
	stream_init(&stream->base, NULL, O_RDONLY, caps);

	stream->underlying = underlying;
	stream->owns_underlying = owns_underlying;
	stream->entry_open = 0;

	/* Create libarchive handle */
	struct archive *a = archive_read_new();
	if (!a)
		return -ENOMEM;

	/* Enable all formats and filters */
	archive_read_support_filter_all(a);
	archive_read_support_format_all(a);

	/* Set up callback data */
	struct archive_read_data *data = calloc(1, sizeof(*data));
	if (!data) {
		archive_read_free(a);
		return -ENOMEM;
	}

	data->underlying = underlying;
	data->error = 0;

	/* Open archive with callbacks */
	int ret = archive_read_open(a, data,
				    NULL,  /* open callback */
				    archive_stream_read_callback,
				    archive_stream_close_callback);
	if (ret != ARCHIVE_OK) {
		/* Note: data is freed by close callback, don't double-free */
		archive_read_free(a);
		return -EIO;
	}

	stream->archive = a;
	return 0;
}

/* Read callback for libarchive */
static la_ssize_t archive_stream_read_callback(struct archive *a,
					       void *client_data,
					       const void **buffer)
{
	struct archive_read_data *data = client_data;
	(void)a;

	*buffer = data->buffer;

	ssize_t nread = stream_read(data->underlying, data->buffer,
				    ARCHIVE_BUFFER_SIZE);
	if (nread < 0) {
		data->error = -nread;
		return -1;
	}

	return nread;
}

/* Close callback for libarchive */
static int archive_stream_close_callback(struct archive *a, void *client_data)
{
	struct archive_read_data *data = client_data;
	(void)a;

	free(data);
	return ARCHIVE_OK;
}

/* Write callbacks for archive creation */
static int stream_archive_write_open(struct archive *a, void *client_data)
{
	(void)a;
	(void)client_data;
	return ARCHIVE_OK;
}

static la_ssize_t stream_archive_write_data(struct archive *a, void *client_data,
					     const void *buffer, size_t length)
{
	struct archive_stream *stream = client_data;
	(void)a;

	ssize_t written = stream_write(stream->underlying, buffer, length);
	if (written < 0)
		return -1;

	return written;
}

static int stream_archive_write_close(struct archive *a, void *client_data)
{
	(void)a;
	(void)client_data;
	return ARCHIVE_OK;
}

/* Walk through all entries in archive */
int archive_stream_walk(struct archive_stream *stream,
			archive_walk_fn callback,
			void *userdata)
{
	struct archive *a = stream->archive;
	struct archive_entry *entry;
	int ret;

	while ((ret = archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
		/* Build entry info */
		struct archive_entry_info info;
		info.pathname = archive_entry_pathname(entry);
		info.name = basename((char *)info.pathname);
		info.size = archive_entry_size(entry);
		info.mode = archive_entry_mode(entry);
		info.mtime = archive_entry_mtime(entry);
		info.is_dir = S_ISDIR(info.mode);
		info.is_compressed = 0;  /* TODO: detect from archive format */

		/* Invoke callback */
		int cb_ret = callback(&info, userdata);
		if (cb_ret != 0)
			return cb_ret;

		/* Skip to next entry */
		archive_read_data_skip(a);
	}

	if (ret != ARCHIVE_EOF)
		return -EIO;

	return 0;
}

/* Open specific entry for reading */
int archive_stream_open_entry(struct archive_stream *stream,
			       const struct archive_entry_info *entry)
{
	struct archive *a = stream->archive;
	struct archive_entry *ae;
	int ret;

	/* If we're looking for a specific entry, we need to iterate until we find it */
	/* This is a simplified implementation - for better performance, we'd cache position */

	/* For now, just mark that an entry is open */
	/* The caller should have positioned the archive correctly via walk */
	stream->entry_open = 1;

	(void)entry;  /* Entry positioning would happen here in full implementation */
	(void)a;
	(void)ae;
	(void)ret;

	return 0;
}

/* Read data from currently open entry */
ssize_t archive_stream_read_data(struct archive_stream *stream,
				  void *buf,
				  size_t count)
{
	if (!stream->entry_open)
		return -EINVAL;

	struct archive *a = stream->archive;
	la_ssize_t nread = archive_read_data(a, buf, count);

	if (nread < 0)
		return -EIO;

	if (nread == 0)
		stream->entry_open = 0;  /* EOF for this entry */

	return nread;
}

/* Open archive for writing */
int archive_stream_open_write(struct archive_stream *stream,
			      struct stream *underlying,
			      enum streamio_archive_format format,
			      int owns_underlying)
{
	memset(stream, 0, sizeof(*stream));

	/* Set write capability */
	unsigned int caps = STREAM_CAP_WRITE;
	stream_init(&stream->base, NULL, O_WRONLY, caps);

	stream->underlying = underlying;
	stream->owns_underlying = owns_underlying;
	stream->is_writing = 1;

	/* Create libarchive write handle */
	struct archive *a = archive_write_new();
	if (!a)
		return -ENOMEM;

	/* Set format based on enum */
	int ret;
	switch (format) {
	case STREAMIO_ARCHIVE_TAR_USTAR:
		ret = archive_write_set_format_ustar(a);
		break;
	case STREAMIO_ARCHIVE_TAR_PAX:
		ret = archive_write_set_format_pax(a);
		break;
	case STREAMIO_ARCHIVE_ZIP:
		ret = archive_write_set_format_zip(a);
		break;
	case STREAMIO_ARCHIVE_7ZIP:
		ret = archive_write_set_format_7zip(a);
		break;
	case STREAMIO_ARCHIVE_CPIO:
		ret = archive_write_set_format_cpio(a);
		break;
	case STREAMIO_ARCHIVE_SHAR:
		ret = archive_write_set_format_shar(a);
		break;
	case STREAMIO_ARCHIVE_ISO9660:
		ret = archive_write_set_format_iso9660(a);
		break;
	default:
		archive_write_free(a);
		return -EINVAL;
	}

	if (ret != ARCHIVE_OK) {
		archive_write_free(a);
		return -EIO;
	}

	/* Open with callbacks */
	ret = archive_write_open(a, stream,
				 stream_archive_write_open,
				 stream_archive_write_data,
				 stream_archive_write_close);
	if (ret != ARCHIVE_OK) {
		archive_write_free(a);
		return -EIO;
	}

	stream->archive = a;
	return 0;
}

/* Create a new entry in the archive */
int archive_stream_new_entry(struct archive_stream *stream,
			     const char *pathname,
			     mode_t mode,
			     off64_t size)
{
	if (!stream->is_writing)
		return -EINVAL;

	if (stream->entry_open)
		return -EBUSY;  /* Must finish previous entry first */

	struct archive_entry *entry = archive_entry_new();
	if (!entry)
		return -ENOMEM;

	archive_entry_set_pathname(entry, pathname);
	archive_entry_set_mode(entry, mode);
	archive_entry_set_size(entry, size);

	int ret = archive_write_header(stream->archive, entry);
	if (ret != ARCHIVE_OK) {
		archive_entry_free(entry);
		return -EIO;
	}

	stream->entry = entry;
	stream->entry_open = 1;
	return 0;
}

/* Write data to current entry */
ssize_t archive_stream_write_data(struct archive_stream *stream,
				  const void *buf,
				  size_t count)
{
	if (!stream->is_writing)
		return -EINVAL;

	if (!stream->entry_open)
		return -EINVAL;  /* Must call new_entry first */

	la_ssize_t written = archive_write_data(stream->archive, buf, count);
	if (written < 0)
		return -EIO;

	return written;
}

/* Finish current entry */
int archive_stream_finish_entry(struct archive_stream *stream)
{
	if (!stream->is_writing)
		return -EINVAL;

	if (!stream->entry_open)
		return -EINVAL;

	int ret = archive_write_finish_entry(stream->archive);
	if (ret != ARCHIVE_OK)
		return -EIO;

	if (stream->entry) {
		archive_entry_free(stream->entry);
		stream->entry = NULL;
	}

	stream->entry_open = 0;
	return 0;
}

/* Check if archive format is available */
int archive_format_available(enum streamio_archive_format format)
{
	/* All formats are available if libarchive is present */
	(void)format;
	return 1;
}

/* Close archive stream */
int archive_stream_close(struct archive_stream *stream)
{
	if (!stream->archive)
		return 0;

	/* Finish any open entry */
	if (stream->entry_open && stream->is_writing) {
		archive_write_finish_entry(stream->archive);
		if (stream->entry) {
			archive_entry_free(stream->entry);
			stream->entry = NULL;
		}
	}

	/* Finalize archive */
	if (stream->is_writing) {
		archive_write_close(stream->archive);
		archive_write_free(stream->archive);
	} else {
		archive_read_free(stream->archive);
	}
	stream->archive = NULL;

	/* Close underlying stream if we own it */
	if (stream->owns_underlying && stream->underlying) {
		stream_close(stream->underlying);
		stream->underlying = NULL;
	}

	return 0;
}

#endif /* HAVE_LIBARCHIVE */
