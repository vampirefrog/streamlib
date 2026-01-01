/**
 * @file mem_stream.c
 * @brief Memory stream implementation
 */

#include "stream.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#ifndef _WIN32
#include <sys/mman.h>
#endif

/* Forward declarations */
static ssize_t mem_stream_read_impl(void *stream, void *buf, size_t count);
static ssize_t mem_stream_write_impl(void *stream, const void *buf, size_t count);
static off64_t mem_stream_seek_impl(void *stream, off64_t offset, int whence);
static off64_t mem_stream_tell_impl(void *stream);
static off64_t mem_stream_size_impl(void *stream);
static void *mem_stream_mmap_impl(void *stream, off64_t start, size_t length,
				  int prot);
static int mem_stream_munmap_impl(void *stream, void *addr, size_t length);
static int mem_stream_close_impl(void *stream);

/* Virtual method table */
static const struct stream_ops mem_stream_ops = {
	.read = mem_stream_read_impl,
	.write = mem_stream_write_impl,
	.seek = mem_stream_seek_impl,
	.tell = mem_stream_tell_impl,
	.size = mem_stream_size_impl,
	.mmap = mem_stream_mmap_impl,
	.munmap = mem_stream_munmap_impl,
	.flush = NULL,  /* No-op for memory streams */
	.close = mem_stream_close_impl,
	.vprintf = NULL,  /* Use default */
	.get_caps = NULL,  /* Use cached caps */
};

/* Initialize with existing buffer */
int mem_stream_init(struct mem_stream *stream, void *buf, size_t size,
		    int writable)
{
	unsigned int caps = STREAM_CAP_READ | STREAM_CAP_SEEK_SET |
			    STREAM_CAP_SEEK_CUR | STREAM_CAP_SEEK_END |
			    STREAM_CAP_TELL | STREAM_CAP_SIZE |
			    STREAM_CAP_MMAP;

	if (writable)
		caps |= STREAM_CAP_WRITE;

	int flags = writable ? O_RDWR : O_RDONLY;

	stream_init(&stream->base, &mem_stream_ops, flags, caps);

	stream->buf = buf;
	stream->size = size;
	stream->capacity = size;
	stream->pos = 0;
	stream->owns_buffer = 0;
	stream->can_grow = 0;

	return 0;
}

/* Initialize growable memory stream (for stack-allocated struct) */
int mem_stream_init_dynamic(struct mem_stream *stream, size_t initial_capacity)
{
	unsigned int caps = STREAM_CAP_READ | STREAM_CAP_WRITE |
			    STREAM_CAP_SEEK_SET | STREAM_CAP_SEEK_CUR |
			    STREAM_CAP_SEEK_END | STREAM_CAP_TELL |
			    STREAM_CAP_SIZE | STREAM_CAP_MMAP;

	stream_init(&stream->base, &mem_stream_ops, O_RDWR, caps);

	if (initial_capacity == 0)
		initial_capacity = 4096;

	stream->buf = malloc(initial_capacity);
	if (!stream->buf)
		return -ENOMEM;

	stream->size = 0;
	stream->capacity = initial_capacity;
	stream->pos = 0;
	stream->owns_buffer = 1;
	stream->can_grow = 1;

	return 0;
}

/* Create new memory stream (allocates struct and buffer) */
struct mem_stream *mem_stream_new(size_t initial_capacity)
{
	struct mem_stream *stream = malloc(sizeof(struct mem_stream));
	if (!stream)
		return NULL;

	if (mem_stream_init_dynamic(stream, initial_capacity) < 0) {
		free(stream);
		return NULL;
	}

	return stream;
}

/* Destroy memory stream created with mem_stream_new */
void mem_stream_destroy(struct mem_stream *stream)
{
	if (!stream)
		return;

	/* Close the stream (frees buffer if owned) */
	stream_close(&stream->base);

	/* Free the struct itself */
	free(stream);
}

/* Get buffer pointer */
const void *mem_stream_get_buffer(struct mem_stream *stream, size_t *size)
{
	if (size)
		*size = stream->size;
	return stream->buf;
}

/* Read implementation */
static ssize_t mem_stream_read_impl(void *stream_ptr, void *buf, size_t count)
{
	struct mem_stream *stream = stream_ptr;

	if (stream->pos >= (off64_t)stream->size)
		return 0;  /* EOF */

	size_t available = stream->size - stream->pos;
	if (count > available)
		count = available;

	memcpy(buf, stream->buf + stream->pos, count);
	stream->pos += count;

	return count;
}

/* Write implementation */
static ssize_t mem_stream_write_impl(void *stream_ptr, const void *buf,
				     size_t count)
{
	struct mem_stream *stream = stream_ptr;

	/* Check if we need to grow the buffer */
	size_t required = stream->pos + count;
	if (required > stream->capacity) {
		if (!stream->can_grow)
			return -ENOSPC;

		/* Grow buffer - double until it fits */
		size_t new_capacity = stream->capacity * 2;
		while (new_capacity < required)
			new_capacity *= 2;

		unsigned char *new_buf = realloc(stream->buf, new_capacity);
		if (!new_buf)
			return -ENOMEM;

		stream->buf = new_buf;
		stream->capacity = new_capacity;
	}

	memcpy(stream->buf + stream->pos, buf, count);
	stream->pos += count;

	/* Update size if we extended past it */
	if (stream->pos > (off64_t)stream->size)
		stream->size = stream->pos;

	return count;
}

/* Seek implementation */
static off64_t mem_stream_seek_impl(void *stream_ptr, off64_t offset, int whence)
{
	struct mem_stream *stream = stream_ptr;
	off64_t new_pos;

	switch (whence) {
	case SEEK_SET:
		new_pos = offset;
		break;
	case SEEK_CUR:
		new_pos = stream->pos + offset;
		break;
	case SEEK_END:
		new_pos = stream->size + offset;
		break;
	default:
		return -EINVAL;
	}

	if (new_pos < 0)
		return -EINVAL;

	stream->pos = new_pos;
	return new_pos;
}

/* Tell implementation */
static off64_t mem_stream_tell_impl(void *stream_ptr)
{
	struct mem_stream *stream = stream_ptr;
	return stream->pos;
}

/* Size implementation */
static off64_t mem_stream_size_impl(void *stream_ptr)
{
	struct mem_stream *stream = stream_ptr;
	return stream->size;
}

/* mmap implementation - just return pointer to buffer */
static void *mem_stream_mmap_impl(void *stream_ptr, off64_t start, size_t length,
				  int prot)
{
	struct mem_stream *stream = stream_ptr;

	/* Verify bounds */
	if (start < 0 || start > (off64_t)stream->size)
		return NULL;

	if (start + length > stream->size)
		return NULL;

	/* Check permissions */
	if ((prot & PROT_WRITE) && !(stream->base.flags & O_RDWR))
		return NULL;

	/* Return direct pointer into buffer */
	return stream->buf + start;
}

/* munmap implementation - no-op for memory streams */
static int mem_stream_munmap_impl(void *stream_ptr, void *addr, size_t length)
{
	(void)stream_ptr;
	(void)addr;
	(void)length;
	return 0;  /* Success - nothing to do */
}

/* Close implementation */
static int mem_stream_close_impl(void *stream_ptr)
{
	struct mem_stream *stream = stream_ptr;

	if (stream->owns_buffer && stream->buf) {
		free(stream->buf);
		stream->buf = NULL;
	}

	return 0;
}
