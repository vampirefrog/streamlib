/**
 * @file stream.c
 * @brief Base stream implementation
 */

#include "stream.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Initialize base stream */
void stream_init(struct stream *stream, const struct stream_ops *ops,
		 int flags, unsigned int caps)
{
	memset(stream, 0, sizeof(*stream));
	stream->ops = ops;
	stream->flags = flags;
	stream->caps = caps;
	stream->error = 0;
	stream->pos = 0;
}

/* Generic I/O operations */
ssize_t stream_read(struct stream *stream, void *buf, size_t count)
{
	if (!stream->ops->read) {
		stream->error = ENOSYS;
		return -ENOSYS;
	}

	ssize_t ret = stream->ops->read(stream, buf, count);
	if (ret < 0)
		stream->error = -ret;
	else if (ret > 0)
		stream->pos += ret;

	return ret;
}

ssize_t stream_write(struct stream *stream, const void *buf, size_t count)
{
	if (!stream->ops->write) {
		stream->error = ENOSYS;
		return -ENOSYS;
	}

	ssize_t ret = stream->ops->write(stream, buf, count);
	if (ret < 0)
		stream->error = -ret;
	else if (ret > 0)
		stream->pos += ret;

	return ret;
}

/* Seeking operations */
off64_t stream_seek(struct stream *stream, off64_t offset, int whence)
{
	if (!stream->ops->seek) {
		stream->error = ENOSYS;
		return -ENOSYS;
	}

	off64_t ret = stream->ops->seek(stream, offset, whence);
	if (ret < 0)
		stream->error = -ret;
	else
		stream->pos = ret;

	return ret;
}

off64_t stream_tell(struct stream *stream)
{
	if (stream->ops->tell)
		return stream->ops->tell(stream);

	/* Fallback to cached position */
	if (stream->caps & STREAM_CAP_TELL)
		return stream->pos;

	stream->error = ENOSYS;
	return -ENOSYS;
}

off64_t stream_size(struct stream *stream)
{
	if (!stream->ops->size) {
		stream->error = ENOSYS;
		return -ENOSYS;
	}

	off64_t ret = stream->ops->size(stream);
	if (ret < 0)
		stream->error = -ret;

	return ret;
}

/* Memory mapping */
void *stream_mmap(struct stream *stream, off64_t start, size_t length, int prot)
{
	if (!stream->ops->mmap) {
		stream->error = ENOSYS;
		return NULL;
	}

	void *ptr = stream->ops->mmap(stream, start, length, prot);
	if (!ptr)
		stream->error = errno;

	return ptr;
}

int stream_munmap(struct stream *stream, void *addr, size_t length)
{
	if (!stream->ops->munmap) {
		stream->error = ENOSYS;
		return -ENOSYS;
	}

	int ret = stream->ops->munmap(stream, addr, length);
	if (ret < 0)
		stream->error = -ret;

	return ret;
}

/* Resource management */
int stream_flush(struct stream *stream)
{
	if (!stream->ops->flush)
		return 0;  /* No-op if not supported */

	int ret = stream->ops->flush(stream);
	if (ret < 0)
		stream->error = -ret;

	return ret;
}

int stream_close(struct stream *stream)
{
	if (!stream->ops->close) {
		stream->error = ENOSYS;
		return -ENOSYS;
	}

	int ret = stream->ops->close(stream);
	if (ret < 0)
		stream->error = -ret;

	return ret;
}

unsigned int stream_get_caps(struct stream *stream)
{
	if (stream->ops->get_caps)
		return stream->ops->get_caps(stream);

	return stream->caps;
}

/* Capability testing helpers */
int stream_can_read(struct stream *stream)
{
	return !!(stream_get_caps(stream) & STREAM_CAP_READ);
}

int stream_can_write(struct stream *stream)
{
	return !!(stream_get_caps(stream) & STREAM_CAP_WRITE);
}

int stream_can_seek(struct stream *stream)
{
	unsigned int caps = stream_get_caps(stream);
	return !!(caps & (STREAM_CAP_SEEK_SET | STREAM_CAP_SEEK_CUR |
			  STREAM_CAP_SEEK_END));
}

int stream_can_mmap(struct stream *stream)
{
	unsigned int caps = stream_get_caps(stream);
	return !!(caps & (STREAM_CAP_MMAP | STREAM_CAP_MMAP_EMULATED));
}

/* Formatted output */
int stream_vprintf(struct stream *stream, const char *fmt, va_list ap)
{
	if (stream->ops->vprintf)
		return stream->ops->vprintf(stream, fmt, ap);

	/* Fallback: format to buffer and write */
	char buf[4096];
	int len = vsnprintf(buf, sizeof(buf), fmt, ap);
	if (len < 0)
		return len;

	if ((size_t)len >= sizeof(buf)) {
		/* Buffer too small, allocate dynamically */
		char *dynbuf = malloc(len + 1);
		if (!dynbuf) {
			stream->error = ENOMEM;
			return -ENOMEM;
		}

		vsnprintf(dynbuf, len + 1, fmt, ap);
		ssize_t written = stream_write(stream, dynbuf, len);
		free(dynbuf);
		return written;
	}

	return stream_write(stream, buf, len);
}

int stream_printf(struct stream *stream, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int ret = stream_vprintf(stream, fmt, ap);
	va_end(ap);
	return ret;
}

/* Feature detection */
unsigned int stream_get_features(void)
{
	unsigned int features = 0;

#ifdef HAVE_ZLIB
	features |= STREAM_FEAT_ZLIB;
#endif

#ifdef HAVE_BZIP2
	features |= STREAM_FEAT_BZIP2;
#endif

#ifdef HAVE_LZMA
	features |= STREAM_FEAT_LZMA;
#endif

#ifdef HAVE_ZSTD
	features |= STREAM_FEAT_ZSTD;
#endif

#ifdef HAVE_LIBARCHIVE
	features |= STREAM_FEAT_LIBARCHIVE;
#endif

#if defined(__unix__) || defined(__APPLE__)
	features |= STREAM_FEAT_MMAP;
#endif

	return features;
}

int stream_has_feature(enum stream_features feature)
{
	return !!(stream_get_features() & feature);
}

const char *stream_get_version(void)
{
	static char version[32];
	snprintf(version, sizeof(version), "%d.%d.%d",
		 STREAM_VERSION_MAJOR,
		 STREAM_VERSION_MINOR,
		 STREAM_VERSION_PATCH);
	return version;
}

const char *stream_get_features_string(void)
{
	static char buf[256];
	char *p = buf;
	int first = 1;

	*p = '\0';

#ifdef HAVE_ZLIB
	p += sprintf(p, "%szlib", first ? "" : ", ");
	first = 0;
#endif

#ifdef HAVE_BZIP2
	p += sprintf(p, "%sbzip2", first ? "" : ", ");
	first = 0;
#endif

#ifdef HAVE_LZMA
	p += sprintf(p, "%slzma", first ? "" : ", ");
	first = 0;
#endif

#ifdef HAVE_ZSTD
	p += sprintf(p, "%szstd", first ? "" : ", ");
	first = 0;
#endif

#ifdef HAVE_LIBARCHIVE
	p += sprintf(p, "%slibarchive", first ? "" : ", ");
	first = 0;
#endif

	if (first)
		sprintf(buf, "none");

	return buf;
}

/* ============================================================================
 * TRANSPARENT DECOMPRESSION
 * ============================================================================ */

#ifdef HAVE_COMPRESSION

/* Prefetch stream for non-seekable streams (used internally) */
struct prefetch_stream_internal {
	struct stream base;
	struct stream *underlying;
	unsigned char buffer[16];
	size_t buffer_size;
	size_t buffer_pos;
	off64_t total_read;
	int owns_underlying;
};

static ssize_t prefetch_read_internal(void *stream_ptr, void *buf, size_t count)
{
	struct prefetch_stream_internal *s = stream_ptr;
	size_t nread = 0;

	/* First, read from buffer if available */
	if (s->buffer_pos < s->buffer_size) {
		size_t available = s->buffer_size - s->buffer_pos;
		size_t to_copy = (count < available) ? count : available;
		memcpy(buf, s->buffer + s->buffer_pos, to_copy);
		s->buffer_pos += to_copy;
		nread = to_copy;
		buf = (char *)buf + to_copy;
		count -= to_copy;
	}

	/* Then read from underlying stream if more data needed */
	if (count > 0) {
		ssize_t ret = stream_read(s->underlying, buf, count);
		if (ret < 0)
			return (nread > 0) ? (ssize_t)nread : ret;
		nread += ret;
	}

	s->total_read += nread;
	return nread;
}

static off64_t prefetch_tell_internal(void *stream_ptr)
{
	struct prefetch_stream_internal *s = stream_ptr;
	return s->total_read;
}

static int prefetch_close_internal(void *stream_ptr)
{
	struct prefetch_stream_internal *s = stream_ptr;
	int ret = 0;

	/* Close underlying stream if we own it */
	if (s->owns_underlying && s->underlying) {
		ret = stream_close(s->underlying);
	}

	/* Free the malloc'd struct */
	free(s);

	return ret;
}

static const struct stream_ops prefetch_ops_internal = {
	.read = prefetch_read_internal,
	.write = NULL,
	.seek = NULL,
	.tell = prefetch_tell_internal,
	.size = NULL,
	.mmap = NULL,
	.munmap = NULL,
	.flush = NULL,
	.close = prefetch_close_internal,
	.vprintf = NULL,
	.get_caps = NULL,
};

/* Automatically detect and decompress a stream
 *
 * This wraps a stream with automatic decompression if compression is detected.
 * Works with both seekable and non-seekable streams!
 *
 * Parameters:
 *   source: The stream to potentially decompress
 *   cs_storage: Storage for compression_stream (only used if compression detected)
 *   owns_source: If 1, closing the wrapper will close the source stream
 *
 * Returns:
 *   Pointer to stream to use (either source or &cs_storage->base)
 *   NULL if error occurred
 *
 * Usage:
 *   struct compression_stream cs;
 *   struct stream *s = stream_auto_decompress(&file_stream.base, &cs, 0);
 *   if (s) {
 *       stream_read(s, buf, size);  // Automatically decompressed!
 *       stream_close(s);
 *   }
 */
struct stream *stream_auto_decompress(struct stream *source,
                                       struct compression_stream *cs_storage,
                                       int owns_source)
{
	if (!source || !cs_storage)
		return NULL;

	/* Check if stream can seek - only use compression_stream_auto for seekable streams */
	int can_seek = stream_can_seek(source);

	if (can_seek) {
		/* Try seekable auto-detection */
		int ret = compression_stream_auto(cs_storage, source, owns_source);

		if (ret == 0) {
			/* Compression detected and initialized successfully */
			return &cs_storage->base;
		}

		/* If no compression detected, stream already sought back, just return source */
		if (ret == -EINVAL) {
			return source;
		}
	}

	/* For non-seekable streams, use buffered detection */
	{
		/* Read magic bytes for detection */
		unsigned char magic[16];
		ssize_t nread = stream_read(source, magic, sizeof(magic));
		int ret;

		if (nread > 0) {
			/* Detect compression type */
			enum compression_type ctype = COMPRESS_NONE;

			if (nread >= 2 && magic[0] == 0x1f && magic[1] == 0x8b) {
				ctype = COMPRESS_GZIP;
			}
#ifdef HAVE_BZIP2
			else if (nread >= 3 && magic[0] == 'B' && magic[1] == 'Z' && magic[2] == 'h') {
				ctype = COMPRESS_BZIP2;
			}
#endif
#ifdef HAVE_LZMA
			else if (nread >= 6 && magic[0] == 0xFD && magic[1] == '7' &&
				 magic[2] == 'z' && magic[3] == 'X' &&
				 magic[4] == 'Z' && magic[5] == 0x00) {
				ctype = COMPRESS_XZ;
			}
#endif
#ifdef HAVE_ZSTD
			else if (nread >= 4 && magic[0] == 0x28 && magic[1] == 0xB5 &&
				 magic[2] == 0x2F && magic[3] == 0xFD) {
				ctype = COMPRESS_ZSTD;
			}
#endif

			/* Create prefetch stream to replay magic bytes */
			struct prefetch_stream_internal *pfs =
				malloc(sizeof(struct prefetch_stream_internal));
			if (!pfs)
				return source;

			stream_init(&pfs->base, &prefetch_ops_internal, O_RDONLY,
				    STREAM_CAP_READ | STREAM_CAP_TELL);
			pfs->underlying = source;
			pfs->buffer_size = nread;
			memcpy(pfs->buffer, magic, nread);
			pfs->buffer_pos = 0;
			pfs->total_read = 0;
			pfs->owns_underlying = owns_source;

			if (ctype != COMPRESS_NONE) {
				/* Initialize compression with prefetch stream */
				ret = compression_stream_init(cs_storage, &pfs->base, ctype,
							       O_RDONLY, 1);  /* Owns prefetch */
				if (ret == 0) {
					/* Success - compression stream now owns prefetch and source */
					return &cs_storage->base;
				}

				/* Failed - clean up */
				free(pfs);
			} else {
				/* No compression detected - return prefetch stream to replay the bytes we read */
				return &pfs->base;
			}
		}
	}

	/* Detection failed - use source stream directly */
	return source;
}
#endif /* HAVE_COMPRESSION */

/* ============================================================================
 * BINARY I/O HELPERS
 * ============================================================================ */

/* Write unsigned 8-bit integer */
int stream_write_u8(struct stream *stream, uint8_t value)
{
	return stream_write(stream, &value, 1) == 1 ? 0 : -1;
}

/* Write signed 8-bit integer */
int stream_write_i8(struct stream *stream, int8_t value)
{
	return stream_write_u8(stream, (uint8_t)value);
}

/* Write unsigned 16-bit integer (little-endian) */
int stream_write_u16_le(struct stream *stream, uint16_t value)
{
	uint8_t buf[2] = {
		value & 0xFF,
		(value >> 8) & 0xFF
	};
	return stream_write(stream, buf, 2) == 2 ? 0 : -1;
}

/* Write unsigned 16-bit integer (big-endian) */
int stream_write_u16_be(struct stream *stream, uint16_t value)
{
	uint8_t buf[2] = {
		(value >> 8) & 0xFF,
		value & 0xFF
	};
	return stream_write(stream, buf, 2) == 2 ? 0 : -1;
}

/* Write signed 16-bit integer (little-endian) */
int stream_write_i16_le(struct stream *stream, int16_t value)
{
	return stream_write_u16_le(stream, (uint16_t)value);
}

/* Write signed 16-bit integer (big-endian) */
int stream_write_i16_be(struct stream *stream, int16_t value)
{
	return stream_write_u16_be(stream, (uint16_t)value);
}

/* Write unsigned 32-bit integer (little-endian) */
int stream_write_u32_le(struct stream *stream, uint32_t value)
{
	uint8_t buf[4] = {
		value & 0xFF,
		(value >> 8) & 0xFF,
		(value >> 16) & 0xFF,
		(value >> 24) & 0xFF
	};
	return stream_write(stream, buf, 4) == 4 ? 0 : -1;
}

/* Write unsigned 32-bit integer (big-endian) */
int stream_write_u32_be(struct stream *stream, uint32_t value)
{
	uint8_t buf[4] = {
		(value >> 24) & 0xFF,
		(value >> 16) & 0xFF,
		(value >> 8) & 0xFF,
		value & 0xFF
	};
	return stream_write(stream, buf, 4) == 4 ? 0 : -1;
}

/* Write signed 32-bit integer (little-endian) */
int stream_write_i32_le(struct stream *stream, int32_t value)
{
	return stream_write_u32_le(stream, (uint32_t)value);
}

/* Write signed 32-bit integer (big-endian) */
int stream_write_i32_be(struct stream *stream, int32_t value)
{
	return stream_write_u32_be(stream, (uint32_t)value);
}

/* Write unsigned 64-bit integer (little-endian) */
int stream_write_u64_le(struct stream *stream, uint64_t value)
{
	uint8_t buf[8];
	for (int i = 0; i < 8; i++)
		buf[i] = (value >> (i * 8)) & 0xFF;
	return stream_write(stream, buf, 8) == 8 ? 0 : -1;
}

/* Write unsigned 64-bit integer (big-endian) */
int stream_write_u64_be(struct stream *stream, uint64_t value)
{
	uint8_t buf[8];
	for (int i = 0; i < 8; i++)
		buf[i] = (value >> (56 - i * 8)) & 0xFF;
	return stream_write(stream, buf, 8) == 8 ? 0 : -1;
}

/* Write float (little-endian) */
int stream_write_float_le(struct stream *stream, float value)
{
	uint32_t bits;
	memcpy(&bits, &value, sizeof(float));
	return stream_write_u32_le(stream, bits);
}

/* Write float (big-endian) */
int stream_write_float_be(struct stream *stream, float value)
{
	uint32_t bits;
	memcpy(&bits, &value, sizeof(float));
	return stream_write_u32_be(stream, bits);
}

/* Write double (little-endian) */
int stream_write_double_le(struct stream *stream, double value)
{
	uint64_t bits;
	memcpy(&bits, &value, sizeof(double));
	return stream_write_u64_le(stream, bits);
}

/* Write double (big-endian) */
int stream_write_double_be(struct stream *stream, double value)
{
	uint64_t bits;
	memcpy(&bits, &value, sizeof(double));
	return stream_write_u64_be(stream, bits);
}

/* Write string with length prefix */
int stream_write_string(struct stream *stream, const char *str)
{
	uint16_t len = strlen(str);
	if (stream_write_u16_le(stream, len) < 0)
		return -1;
	return stream_write(stream, str, len) == (ssize_t)len ? 0 : -1;
}

/* Read unsigned 8-bit integer */
int stream_read_u8(struct stream *stream, uint8_t *value)
{
	return stream_read(stream, value, 1) == 1 ? 0 : -1;
}

/* Read signed 8-bit integer */
int stream_read_i8(struct stream *stream, int8_t *value)
{
	return stream_read_u8(stream, (uint8_t *)value);
}

/* Read unsigned 16-bit integer (little-endian) */
int stream_read_u16_le(struct stream *stream, uint16_t *value)
{
	uint8_t buf[2];
	if (stream_read(stream, buf, 2) != 2)
		return -1;
	*value = buf[0] | (buf[1] << 8);
	return 0;
}

/* Read unsigned 16-bit integer (big-endian) */
int stream_read_u16_be(struct stream *stream, uint16_t *value)
{
	uint8_t buf[2];
	if (stream_read(stream, buf, 2) != 2)
		return -1;
	*value = (buf[0] << 8) | buf[1];
	return 0;
}

/* Read signed 16-bit integer (little-endian) */
int stream_read_i16_le(struct stream *stream, int16_t *value)
{
	return stream_read_u16_le(stream, (uint16_t *)value);
}

/* Read signed 16-bit integer (big-endian) */
int stream_read_i16_be(struct stream *stream, int16_t *value)
{
	return stream_read_u16_be(stream, (uint16_t *)value);
}

/* Read unsigned 32-bit integer (little-endian) */
int stream_read_u32_le(struct stream *stream, uint32_t *value)
{
	uint8_t buf[4];
	if (stream_read(stream, buf, 4) != 4)
		return -1;
	*value = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
	return 0;
}

/* Read unsigned 32-bit integer (big-endian) */
int stream_read_u32_be(struct stream *stream, uint32_t *value)
{
	uint8_t buf[4];
	if (stream_read(stream, buf, 4) != 4)
		return -1;
	*value = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
	return 0;
}

/* Read signed 32-bit integer (little-endian) */
int stream_read_i32_le(struct stream *stream, int32_t *value)
{
	return stream_read_u32_le(stream, (uint32_t *)value);
}

/* Read signed 32-bit integer (big-endian) */
int stream_read_i32_be(struct stream *stream, int32_t *value)
{
	return stream_read_u32_be(stream, (uint32_t *)value);
}

/* Read unsigned 64-bit integer (little-endian) */
int stream_read_u64_le(struct stream *stream, uint64_t *value)
{
	uint8_t buf[8];
	if (stream_read(stream, buf, 8) != 8)
		return -1;
	*value = 0;
	for (int i = 0; i < 8; i++)
		*value |= ((uint64_t)buf[i]) << (i * 8);
	return 0;
}

/* Read unsigned 64-bit integer (big-endian) */
int stream_read_u64_be(struct stream *stream, uint64_t *value)
{
	uint8_t buf[8];
	if (stream_read(stream, buf, 8) != 8)
		return -1;
	*value = 0;
	for (int i = 0; i < 8; i++)
		*value |= ((uint64_t)buf[i]) << (56 - i * 8);
	return 0;
}

/* Read float (little-endian) */
int stream_read_float_le(struct stream *stream, float *value)
{
	uint32_t bits;
	if (stream_read_u32_le(stream, &bits) < 0)
		return -1;
	memcpy(value, &bits, sizeof(float));
	return 0;
}

/* Read float (big-endian) */
int stream_read_float_be(struct stream *stream, float *value)
{
	uint32_t bits;
	if (stream_read_u32_be(stream, &bits) < 0)
		return -1;
	memcpy(value, &bits, sizeof(float));
	return 0;
}

/* Read double (little-endian) */
int stream_read_double_le(struct stream *stream, double *value)
{
	uint64_t bits;
	if (stream_read_u64_le(stream, &bits) < 0)
		return -1;
	memcpy(value, &bits, sizeof(double));
	return 0;
}

/* Read double (big-endian) */
int stream_read_double_be(struct stream *stream, double *value)
{
	uint64_t bits;
	if (stream_read_u64_be(stream, &bits) < 0)
		return -1;
	memcpy(value, &bits, sizeof(double));
	return 0;
}

/* Read string with length prefix */
int stream_read_string(struct stream *stream, char *buf, size_t buf_size)
{
	uint16_t len;
	if (stream_read_u16_le(stream, &len) < 0)
		return -1;

	if (len >= buf_size)
		return -1;  /* Buffer too small */

	if (stream_read(stream, buf, len) != (ssize_t)len)
		return -1;

	buf[len] = '\0';
	return 0;
}
