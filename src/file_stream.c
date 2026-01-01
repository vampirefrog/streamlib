/**
 * @file file_stream.c
 * @brief File stream implementation (POSIX)
 */

#include "stream.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

/* Forward declarations */
static ssize_t file_stream_read_impl(void *stream, void *buf, size_t count);
static ssize_t file_stream_write_impl(void *stream, const void *buf, size_t count);
static off64_t file_stream_seek_impl(void *stream, off64_t offset, int whence);
static off64_t file_stream_tell_impl(void *stream);
static off64_t file_stream_size_impl(void *stream);
static void *file_stream_mmap_impl(void *stream, off64_t start, size_t length,
				   int prot);
static int file_stream_munmap_impl(void *stream, void *addr, size_t length);
static int file_stream_flush_impl(void *stream);
static int file_stream_close_impl(void *stream);

/* Virtual method table */
static const struct stream_ops file_stream_ops = {
	.read = file_stream_read_impl,
	.write = file_stream_write_impl,
	.seek = file_stream_seek_impl,
	.tell = file_stream_tell_impl,
	.size = file_stream_size_impl,
	.mmap = file_stream_mmap_impl,
	.munmap = file_stream_munmap_impl,
	.flush = file_stream_flush_impl,
	.close = file_stream_close_impl,
	.vprintf = NULL,  /* Use default */
	.get_caps = NULL,  /* Use cached caps */
};

/* Open file stream */
int file_stream_open(struct file_stream *stream, const char *path, int flags,
		     mode_t mode)
{
	memset(stream, 0, sizeof(*stream));

	/* Open file */
	stream->fd = open(path, flags | O_LARGEFILE, mode);
	if (stream->fd < 0)
		return -errno;

	/* Determine capabilities */
	unsigned int caps = STREAM_CAP_SEEK_SET | STREAM_CAP_SEEK_CUR |
			    STREAM_CAP_SEEK_END | STREAM_CAP_TELL |
			    STREAM_CAP_SIZE | STREAM_CAP_MMAP |
			    STREAM_CAP_FLUSH;

	if ((flags & O_ACCMODE) == O_RDONLY || (flags & O_ACCMODE) == O_RDWR)
		caps |= STREAM_CAP_READ;

	if ((flags & O_ACCMODE) == O_WRONLY || (flags & O_ACCMODE) == O_RDWR)
		caps |= STREAM_CAP_WRITE;

	stream_init(&stream->base, &file_stream_ops, flags, caps);

	/* Save path for debugging */
	stream->path = strdup(path);

	stream->mmap_addr = NULL;
	stream->mmap_size = 0;
	stream->mmap_start = 0;

	return 0;
}

/* Initialize from existing fd */
int file_stream_from_fd(struct file_stream *stream, int fd, int flags)
{
	memset(stream, 0, sizeof(*stream));

	stream->fd = fd;

	/* Determine capabilities */
	unsigned int caps = STREAM_CAP_SEEK_SET | STREAM_CAP_SEEK_CUR |
			    STREAM_CAP_SEEK_END | STREAM_CAP_TELL |
			    STREAM_CAP_SIZE | STREAM_CAP_MMAP |
			    STREAM_CAP_FLUSH;

	if ((flags & O_ACCMODE) == O_RDONLY || (flags & O_ACCMODE) == O_RDWR)
		caps |= STREAM_CAP_READ;

	if ((flags & O_ACCMODE) == O_WRONLY || (flags & O_ACCMODE) == O_RDWR)
		caps |= STREAM_CAP_WRITE;

	stream_init(&stream->base, &file_stream_ops, flags, caps);

	stream->path = NULL;
	stream->mmap_addr = NULL;
	stream->mmap_size = 0;
	stream->mmap_start = 0;

	return 0;
}

/* Read implementation */
static ssize_t file_stream_read_impl(void *stream_ptr, void *buf, size_t count)
{
	struct file_stream *stream = stream_ptr;
	ssize_t ret = read(stream->fd, buf, count);
	if (ret < 0)
		return -errno;
	return ret;
}

/* Write implementation */
static ssize_t file_stream_write_impl(void *stream_ptr, const void *buf,
				      size_t count)
{
	struct file_stream *stream = stream_ptr;
	ssize_t ret = write(stream->fd, buf, count);
	if (ret < 0)
		return -errno;
	return ret;
}

/* Seek implementation */
static off64_t file_stream_seek_impl(void *stream_ptr, off64_t offset, int whence)
{
	struct file_stream *stream = stream_ptr;
	off64_t ret = lseek(stream->fd, offset, whence);
	if (ret < 0)
		return -errno;
	return ret;
}

/* Tell implementation */
static off64_t file_stream_tell_impl(void *stream_ptr)
{
	struct file_stream *stream = stream_ptr;
	off64_t ret = lseek(stream->fd, 0, SEEK_CUR);
	if (ret < 0)
		return -errno;
	return ret;
}

/* Size implementation */
static off64_t file_stream_size_impl(void *stream_ptr)
{
	struct file_stream *stream = stream_ptr;
	struct stat st;

	if (fstat(stream->fd, &st) < 0)
		return -errno;

	return st.st_size;
}

/* mmap implementation */
static void *file_stream_mmap_impl(void *stream_ptr, off64_t start, size_t length,
				   int prot)
{
	struct file_stream *stream = stream_ptr;

	/* Unmap any existing mapping first */
	if (stream->mmap_addr) {
		munmap(stream->mmap_addr, stream->mmap_size);
		stream->mmap_addr = NULL;
	}

	/* Determine protection flags */
	int prot_flags = 0;
	if (prot & PROT_READ)
		prot_flags |= PROT_READ;
	if (prot & PROT_WRITE)
		prot_flags |= PROT_WRITE;

	/* Map the file */
	void *addr = mmap(NULL, length, prot_flags, MAP_PRIVATE, stream->fd, start);
	if (addr == MAP_FAILED)
		return NULL;

	/* Save mapping info for cleanup */
	stream->mmap_addr = addr;
	stream->mmap_size = length;
	stream->mmap_start = start;

	return addr;
}

/* munmap implementation */
static int file_stream_munmap_impl(void *stream_ptr, void *addr, size_t length)
{
	struct file_stream *stream = stream_ptr;

	/* Verify this is our mapped region */
	if (addr != stream->mmap_addr || length != stream->mmap_size)
		return -EINVAL;

	if (munmap(addr, length) < 0)
		return -errno;

	stream->mmap_addr = NULL;
	stream->mmap_size = 0;
	stream->mmap_start = 0;

	return 0;
}

/* Flush implementation */
static int file_stream_flush_impl(void *stream_ptr)
{
	struct file_stream *stream = stream_ptr;

	if (fsync(stream->fd) < 0)
		return -errno;

	return 0;
}

/* Close implementation */
static int file_stream_close_impl(void *stream_ptr)
{
	struct file_stream *stream = stream_ptr;

	/* Unmap if needed */
	if (stream->mmap_addr) {
		munmap(stream->mmap_addr, stream->mmap_size);
		stream->mmap_addr = NULL;
	}

	/* Close fd */
	if (stream->fd >= 0) {
		close(stream->fd);
		stream->fd = -1;
	}

	/* Free path */
	if (stream->path) {
		free(stream->path);
		stream->path = NULL;
	}

	return 0;
}
