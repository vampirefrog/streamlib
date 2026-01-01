/**
 * @file compression_stream.c
 * @brief Compression stream implementation
 */

#include "stream.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#ifdef STREAM_HAVE_ZLIB
#include <zlib.h>

/* zlib state structure */
struct zlib_state {
	z_stream strm;
	int initialized;
};
#endif

#ifdef STREAM_HAVE_BZIP2
#include <bzlib.h>

/* bzip2 state structure */
struct bzip2_state {
	bz_stream strm;
	int initialized;
};
#endif

#ifdef STREAM_HAVE_LZMA
#include <lzma.h>

/* lzma state structure */
struct lzma_state {
	lzma_stream strm;
	int initialized;
};
#endif

#ifdef STREAM_HAVE_ZSTD
#include <zstd.h>

/* zstd state structure */
struct zstd_state {
	union {
		ZSTD_CStream *cstream;  /* Compression stream */
		ZSTD_DStream *dstream;  /* Decompression stream */
	};
	int initialized;
	int is_writing;
};
#endif

#if defined(STREAM_HAVE_ZLIB) || defined(STREAM_HAVE_BZIP2) || defined(STREAM_HAVE_LZMA) || defined(STREAM_HAVE_ZSTD)

/* Forward declarations */
static ssize_t compression_stream_read_impl(void *stream, void *buf, size_t count);
static ssize_t compression_stream_write_impl(void *stream, const void *buf, size_t count);
static void *compression_stream_mmap_impl(void *stream, off64_t start, size_t length, int prot);
static int compression_stream_munmap_impl(void *stream, void *addr, size_t length);
static int compression_stream_close_impl(void *stream);

/* Virtual method table */
static const struct stream_ops compression_stream_ops = {
	.read = compression_stream_read_impl,
	.write = compression_stream_write_impl,
	.seek = NULL,  /* Compressed streams generally can't seek */
	.tell = NULL,
	.size = NULL,
	.mmap = compression_stream_mmap_impl,  /* Emulated mmap */
	.munmap = compression_stream_munmap_impl,
	.flush = NULL,
	.close = compression_stream_close_impl,
	.vprintf = NULL,
	.get_caps = NULL,
};

/* Check if compression type is available */
int compression_is_available(enum compression_type type)
{
	switch (type) {
	case COMPRESS_NONE:
		return 1;

#ifdef STREAM_HAVE_ZLIB
	case COMPRESS_GZIP:
	case COMPRESS_ZLIB:
		return 1;
#endif

#ifdef STREAM_HAVE_BZIP2
	case COMPRESS_BZIP2:
		return 1;
#endif

#ifdef STREAM_HAVE_LZMA
	case COMPRESS_XZ:
	case COMPRESS_LZMA:
		return 1;
#endif

#ifdef STREAM_HAVE_ZSTD
	case COMPRESS_ZSTD:
		return 1;
#endif

	default:
		return 0;
	}
}

/* Initialize codec-specific state */
static int init_codec(struct compression_stream *stream)
{
	int ret;

	switch (stream->type) {
#ifdef STREAM_HAVE_ZLIB
	case COMPRESS_GZIP:
	case COMPRESS_ZLIB: {
		struct zlib_state *zs = calloc(1, sizeof(*zs));
		if (!zs)
			return -ENOMEM;
		stream->codec_state = zs;

		int window_bits = (stream->type == COMPRESS_GZIP) ? (15 + 16) : 15;

		if (stream->is_writing) {
			ret = deflateInit2(&zs->strm, Z_DEFAULT_COMPRESSION,
					   Z_DEFLATED, window_bits, 8,
					   Z_DEFAULT_STRATEGY);
		} else {
			ret = inflateInit2(&zs->strm, window_bits);
		}

		if (ret != Z_OK) {
			free(zs);
			return -EIO;
		}
		zs->initialized = 1;
		return 0;
	}
#endif

#ifdef STREAM_HAVE_BZIP2
	case COMPRESS_BZIP2: {
		struct bzip2_state *bs = calloc(1, sizeof(*bs));
		if (!bs)
			return -ENOMEM;
		stream->codec_state = bs;

		if (stream->is_writing) {
			ret = BZ2_bzCompressInit(&bs->strm, 9, 0, 30);
		} else {
			ret = BZ2_bzDecompressInit(&bs->strm, 0, 0);
		}

		if (ret != BZ_OK) {
			free(bs);
			return -EIO;
		}
		bs->initialized = 1;
		return 0;
	}
#endif

#ifdef STREAM_HAVE_LZMA
	case COMPRESS_XZ:
	case COMPRESS_LZMA: {
		struct lzma_state *ls = calloc(1, sizeof(*ls));
		if (!ls)
			return -ENOMEM;
		stream->codec_state = ls;

		ls->strm = (lzma_stream)LZMA_STREAM_INIT;

		if (stream->is_writing) {
			if (stream->type == COMPRESS_XZ)
				ret = lzma_easy_encoder(&ls->strm, 6, LZMA_CHECK_CRC64);
			else
				ret = lzma_alone_encoder(&ls->strm, NULL);
		} else {
			uint64_t memlimit = UINT64_MAX;
			ret = lzma_auto_decoder(&ls->strm, memlimit, 0);
		}

		if (ret != LZMA_OK) {
			free(ls);
			return -EIO;
		}
		ls->initialized = 1;
		return 0;
	}
#endif

#ifdef STREAM_HAVE_ZSTD
	case COMPRESS_ZSTD: {
		struct zstd_state *zs = calloc(1, sizeof(*zs));
		if (!zs)
			return -ENOMEM;
		stream->codec_state = zs;

		zs->is_writing = stream->is_writing;

		if (stream->is_writing) {
			zs->cstream = ZSTD_createCStream();
			if (!zs->cstream) {
				free(zs);
				return -ENOMEM;
			}
			size_t ret = ZSTD_initCStream(zs->cstream, 3);  /* compression level 3 */
			if (ZSTD_isError(ret)) {
				ZSTD_freeCStream(zs->cstream);
				free(zs);
				return -EIO;
			}
		} else {
			zs->dstream = ZSTD_createDStream();
			if (!zs->dstream) {
				free(zs);
				return -ENOMEM;
			}
			size_t ret = ZSTD_initDStream(zs->dstream);
			if (ZSTD_isError(ret)) {
				ZSTD_freeDStream(zs->dstream);
				free(zs);
				return -EIO;
			}
		}
		zs->initialized = 1;
		return 0;
	}
#endif

	default:
		return -ENOSYS;
	}
}

/* Initialize compression stream */
int compression_stream_init(struct compression_stream *stream,
			     struct stream *underlying,
			     enum compression_type type,
			     int flags,
			     int owns_underlying)
{
	if (!compression_is_available(type))
		return -ENOSYS;

	memset(stream, 0, sizeof(*stream));

	/* Determine capabilities - compressed streams are limited */
	unsigned int caps = STREAM_CAP_COMPRESSED | STREAM_CAP_MMAP_EMULATED;

	/* Reading compressed data */
	if ((flags & O_ACCMODE) == O_RDONLY) {
		caps |= STREAM_CAP_READ;
		stream->is_writing = 0;
	}
	/* Writing compressed data */
	else if ((flags & O_ACCMODE) == O_WRONLY) {
		caps |= STREAM_CAP_WRITE;
		stream->is_writing = 1;
	} else {
		return -EINVAL;  /* Compression streams are one-way */
	}

	stream_init(&stream->base, &compression_stream_ops, flags, caps);

	stream->underlying = underlying;
	stream->owns_underlying = owns_underlying;
	stream->type = type;
	stream->at_eof = 0;
	stream->outbuf_used = 0;
	stream->outbuf_pos = 0;

	/* Initialize codec-specific state */
	return init_codec(stream);
}

/* Convenience wrapper for gzip */
int gzip_stream_init(struct compression_stream *stream,
		     struct stream *underlying,
		     int flags,
		     int owns_underlying)
{
	return compression_stream_init(stream, underlying, COMPRESS_GZIP,
				       flags, owns_underlying);
}

/* Read implementation - decompress data */
static ssize_t compression_stream_read_impl(void *stream_ptr, void *buf,
					    size_t count)
{
	struct compression_stream *stream = stream_ptr;

	if (stream->at_eof)
		return 0;

	/* Return buffered data first */
	if (stream->outbuf_pos < stream->outbuf_used) {
		size_t available = stream->outbuf_used - stream->outbuf_pos;
		size_t to_copy = (count < available) ? count : available;
		memcpy(buf, stream->outbuf + stream->outbuf_pos, to_copy);
		stream->outbuf_pos += to_copy;
		return to_copy;
	}

	/* Need to decompress more data */
	stream->outbuf_pos = 0;
	stream->outbuf_used = 0;

	/* Codec-specific decompression */
	switch (stream->type) {
#ifdef STREAM_HAVE_ZLIB
	case COMPRESS_GZIP:
	case COMPRESS_ZLIB: {
		struct zlib_state *zs = stream->codec_state;
		zs->strm.next_out = stream->outbuf;
		zs->strm.avail_out = sizeof(stream->outbuf);

		while (zs->strm.avail_out > 0) {
			if (zs->strm.avail_in == 0) {
				ssize_t nread = stream_read(stream->underlying,
							    stream->inbuf,
							    sizeof(stream->inbuf));
				if (nread < 0)
					return nread;
				if (nread == 0) {
					int ret = inflate(&zs->strm, Z_FINISH);
					if (ret == Z_STREAM_END)
						stream->at_eof = 1;
					break;
				}
				zs->strm.next_in = stream->inbuf;
				zs->strm.avail_in = nread;
			}

			int ret = inflate(&zs->strm, Z_NO_FLUSH);
			if (ret == Z_STREAM_END) {
				stream->at_eof = 1;
				break;
			}
			if (ret != Z_OK && ret != Z_BUF_ERROR)
				return -EIO;

			if (zs->strm.avail_out < sizeof(stream->outbuf))
				break;
		}
		stream->outbuf_used = sizeof(stream->outbuf) - zs->strm.avail_out;
		break;
	}
#endif

#ifdef STREAM_HAVE_BZIP2
	case COMPRESS_BZIP2: {
		struct bzip2_state *bs = stream->codec_state;
		bs->strm.next_out = (char *)stream->outbuf;
		bs->strm.avail_out = sizeof(stream->outbuf);

		while (bs->strm.avail_out > 0) {
			if (bs->strm.avail_in == 0) {
				ssize_t nread = stream_read(stream->underlying,
							    stream->inbuf,
							    sizeof(stream->inbuf));
				if (nread < 0)
					return nread;
				if (nread == 0) {
					stream->at_eof = 1;
					break;
				}
				bs->strm.next_in = (char *)stream->inbuf;
				bs->strm.avail_in = nread;
			}

			int ret = BZ2_bzDecompress(&bs->strm);
			if (ret == BZ_STREAM_END) {
				stream->at_eof = 1;
				break;
			}
			if (ret != BZ_OK)
				return -EIO;

			if (bs->strm.avail_out < sizeof(stream->outbuf))
				break;
		}
		stream->outbuf_used = sizeof(stream->outbuf) - bs->strm.avail_out;
		break;
	}
#endif

#ifdef STREAM_HAVE_LZMA
	case COMPRESS_XZ:
	case COMPRESS_LZMA: {
		struct lzma_state *ls = stream->codec_state;
		ls->strm.next_out = stream->outbuf;
		ls->strm.avail_out = sizeof(stream->outbuf);

		while (ls->strm.avail_out > 0) {
			if (ls->strm.avail_in == 0) {
				ssize_t nread = stream_read(stream->underlying,
							    stream->inbuf,
							    sizeof(stream->inbuf));
				if (nread < 0)
					return nread;
				if (nread == 0) {
					stream->at_eof = 1;
					break;
				}
				ls->strm.next_in = stream->inbuf;
				ls->strm.avail_in = nread;
			}

			lzma_ret ret = lzma_code(&ls->strm, LZMA_RUN);
			if (ret == LZMA_STREAM_END) {
				stream->at_eof = 1;
				break;
			}
			if (ret != LZMA_OK)
				return -EIO;

			if (ls->strm.avail_out < sizeof(stream->outbuf))
				break;
		}
		stream->outbuf_used = sizeof(stream->outbuf) - ls->strm.avail_out;
		break;
	}
#endif

#ifdef STREAM_HAVE_ZSTD
	case COMPRESS_ZSTD: {
		struct zstd_state *zs = stream->codec_state;

		ZSTD_outBuffer output = {
			.dst = stream->outbuf,
			.size = sizeof(stream->outbuf),
			.pos = 0
		};

		while (output.pos < output.size) {
			ZSTD_inBuffer input = {
				.src = stream->inbuf,
				.size = 0,
				.pos = 0
			};

			/* Read more compressed data if needed */
			if (input.size == 0) {
				ssize_t nread = stream_read(stream->underlying,
							    stream->inbuf,
							    sizeof(stream->inbuf));
				if (nread < 0)
					return nread;
				if (nread == 0) {
					stream->at_eof = 1;
					break;
				}
				input.size = nread;
			}

			size_t ret = ZSTD_decompressStream(zs->dstream, &output, &input);
			if (ZSTD_isError(ret))
				return -EIO;

			if (ret == 0) {
				/* Frame fully decoded */
				stream->at_eof = 1;
				break;
			}

			if (output.pos > 0)
				break;
		}

		stream->outbuf_used = output.pos;
		break;
	}
#endif

	default:
		return -ENOSYS;
	}

	if (stream->outbuf_used == 0)
		return 0;  /* EOF */

	/* Copy to output buffer */
	size_t to_copy = (count < stream->outbuf_used) ? count : stream->outbuf_used;
	memcpy(buf, stream->outbuf, to_copy);
	stream->outbuf_pos = to_copy;

	return to_copy;
}

/* Write implementation - compress data */
static ssize_t compression_stream_write_impl(void *stream_ptr, const void *buf,
					     size_t count)
{
	struct compression_stream *stream = stream_ptr;

	switch (stream->type) {
#ifdef STREAM_HAVE_ZLIB
	case COMPRESS_GZIP:
	case COMPRESS_ZLIB: {
		struct zlib_state *zs = stream->codec_state;
		zs->strm.next_in = (unsigned char *)buf;
		zs->strm.avail_in = count;

		while (zs->strm.avail_in > 0) {
			zs->strm.next_out = stream->outbuf;
			zs->strm.avail_out = sizeof(stream->outbuf);

			int ret = deflate(&zs->strm, Z_NO_FLUSH);
			if (ret != Z_OK)
				return -EIO;

			size_t compressed = sizeof(stream->outbuf) - zs->strm.avail_out;
			if (compressed > 0) {
				ssize_t written = stream_write(stream->underlying,
							       stream->outbuf,
							       compressed);
				if (written < 0)
					return written;
				if ((size_t)written != compressed)
					return -EIO;
			}
		}
		break;
	}
#endif

#ifdef STREAM_HAVE_BZIP2
	case COMPRESS_BZIP2: {
		struct bzip2_state *bs = stream->codec_state;
		bs->strm.next_in = (char *)buf;
		bs->strm.avail_in = count;

		while (bs->strm.avail_in > 0) {
			bs->strm.next_out = (char *)stream->outbuf;
			bs->strm.avail_out = sizeof(stream->outbuf);

			int ret = BZ2_bzCompress(&bs->strm, BZ_RUN);
			if (ret != BZ_RUN_OK)
				return -EIO;

			size_t compressed = sizeof(stream->outbuf) - bs->strm.avail_out;
			if (compressed > 0) {
				ssize_t written = stream_write(stream->underlying,
							       stream->outbuf,
							       compressed);
				if (written < 0)
					return written;
				if ((size_t)written != compressed)
					return -EIO;
			}
		}
		break;
	}
#endif

#ifdef STREAM_HAVE_LZMA
	case COMPRESS_XZ:
	case COMPRESS_LZMA: {
		struct lzma_state *ls = stream->codec_state;
		ls->strm.next_in = buf;
		ls->strm.avail_in = count;

		while (ls->strm.avail_in > 0) {
			ls->strm.next_out = stream->outbuf;
			ls->strm.avail_out = sizeof(stream->outbuf);

			lzma_ret ret = lzma_code(&ls->strm, LZMA_RUN);
			if (ret != LZMA_OK)
				return -EIO;

			size_t compressed = sizeof(stream->outbuf) - ls->strm.avail_out;
			if (compressed > 0) {
				ssize_t written = stream_write(stream->underlying,
							       stream->outbuf,
							       compressed);
				if (written < 0)
					return written;
				if ((size_t)written != compressed)
					return -EIO;
			}
		}
		break;
	}
#endif

#ifdef STREAM_HAVE_ZSTD
	case COMPRESS_ZSTD: {
		struct zstd_state *zs = stream->codec_state;

		ZSTD_inBuffer input = {
			.src = buf,
			.size = count,
			.pos = 0
		};

		while (input.pos < input.size) {
			ZSTD_outBuffer output = {
				.dst = stream->outbuf,
				.size = sizeof(stream->outbuf),
				.pos = 0
			};

			size_t ret = ZSTD_compressStream(zs->cstream, &output, &input);
			if (ZSTD_isError(ret))
				return -EIO;

			/* Write compressed data to underlying stream */
			if (output.pos > 0) {
				ssize_t written = stream_write(stream->underlying,
							       stream->outbuf,
							       output.pos);
				if (written < 0)
					return written;
				if ((size_t)written != output.pos)
					return -EIO;
			}
		}
		break;
	}
#endif

	default:
		return -ENOSYS;
	}

	return count;
}

/* Close implementation */
/* Emulated mmap - read entire stream into memory */
static void *compression_stream_mmap_impl(void *stream_ptr, off64_t start,
					  size_t length, int prot)
{
	struct compression_stream *stream = stream_ptr;
	(void)prot;  /* Protection flags not used for emulated mmap */

	/* Only support read mode for compressed streams */
	if (stream->is_writing)
		return NULL;

	/* Free any existing mapping */
	if (stream->emulated_mmap_addr) {
		free(stream->emulated_mmap_addr);
		stream->emulated_mmap_addr = NULL;
	}

	/* Allocate buffer to hold the data */
	void *buffer = malloc(length);
	if (!buffer)
		return NULL;

	/* Read the requested data */
	ssize_t nread = stream_read(&stream->base, buffer, length);
	if (nread < 0) {
		free(buffer);
		return NULL;
	}

	/* If we read less than requested, that's OK (EOF) */
	/* Save the mapping info */
	stream->emulated_mmap_addr = buffer;
	stream->emulated_mmap_size = nread;
	stream->emulated_mmap_start = start;

	return buffer;
}

/* Emulated munmap - free the allocated buffer */
static int compression_stream_munmap_impl(void *stream_ptr, void *addr,
					  size_t length)
{
	struct compression_stream *stream = stream_ptr;
	(void)length;  /* Length can differ from actual read size, that's OK */

	/* Verify this is our mapped region */
	if (addr != stream->emulated_mmap_addr)
		return -EINVAL;

	/* Free the buffer */
	free(stream->emulated_mmap_addr);
	stream->emulated_mmap_addr = NULL;
	stream->emulated_mmap_size = 0;
	stream->emulated_mmap_start = 0;

	return 0;
}

static int compression_stream_close_impl(void *stream_ptr)
{
	struct compression_stream *stream = stream_ptr;

	if (!stream->codec_state)
		return 0;

	switch (stream->type) {
#ifdef STREAM_HAVE_ZLIB
	case COMPRESS_GZIP:
	case COMPRESS_ZLIB: {
		struct zlib_state *zs = stream->codec_state;

		/* Finish compression if writing */
		if (stream->is_writing && zs->initialized) {
			zs->strm.next_in = NULL;
			zs->strm.avail_in = 0;

			int ret;
			do {
				zs->strm.next_out = stream->outbuf;
				zs->strm.avail_out = sizeof(stream->outbuf);

				ret = deflate(&zs->strm, Z_FINISH);

				size_t compressed = sizeof(stream->outbuf) - zs->strm.avail_out;
				if (compressed > 0) {
					stream_write(stream->underlying, stream->outbuf,
						     compressed);
				}
			} while (ret == Z_OK);
		}

		/* Clean up zlib state */
		if (zs->initialized) {
			if (stream->is_writing)
				deflateEnd(&zs->strm);
			else
				inflateEnd(&zs->strm);
		}

		free(zs);
		break;
	}
#endif

#ifdef STREAM_HAVE_BZIP2
	case COMPRESS_BZIP2: {
		struct bzip2_state *bs = stream->codec_state;

		/* Finish compression if writing */
		if (stream->is_writing && bs->initialized) {
			bs->strm.next_in = NULL;
			bs->strm.avail_in = 0;

			int ret;
			do {
				bs->strm.next_out = (char *)stream->outbuf;
				bs->strm.avail_out = sizeof(stream->outbuf);

				ret = BZ2_bzCompress(&bs->strm, BZ_FINISH);

				size_t compressed = sizeof(stream->outbuf) - bs->strm.avail_out;
				if (compressed > 0) {
					stream_write(stream->underlying, stream->outbuf,
						     compressed);
				}
			} while (ret == BZ_FINISH_OK);
		}

		/* Clean up bzip2 state */
		if (bs->initialized) {
			if (stream->is_writing)
				BZ2_bzCompressEnd(&bs->strm);
			else
				BZ2_bzDecompressEnd(&bs->strm);
		}

		free(bs);
		break;
	}
#endif

#ifdef STREAM_HAVE_LZMA
	case COMPRESS_XZ:
	case COMPRESS_LZMA: {
		struct lzma_state *ls = stream->codec_state;

		/* Finish compression if writing */
		if (stream->is_writing && ls->initialized) {
			ls->strm.next_in = NULL;
			ls->strm.avail_in = 0;

			lzma_ret ret;
			do {
				ls->strm.next_out = stream->outbuf;
				ls->strm.avail_out = sizeof(stream->outbuf);

				ret = lzma_code(&ls->strm, LZMA_FINISH);

				size_t compressed = sizeof(stream->outbuf) - ls->strm.avail_out;
				if (compressed > 0) {
					stream_write(stream->underlying, stream->outbuf,
						     compressed);
				}
			} while (ret == LZMA_OK);
		}

		/* Clean up lzma state */
		if (ls->initialized) {
			lzma_end(&ls->strm);
		}

		free(ls);
		break;
	}
#endif

#ifdef STREAM_HAVE_ZSTD
	case COMPRESS_ZSTD: {
		struct zstd_state *zs = stream->codec_state;

		/* Finish compression if writing */
		if (stream->is_writing && zs->initialized) {
			ZSTD_outBuffer output = {
				.dst = stream->outbuf,
				.size = sizeof(stream->outbuf),
				.pos = 0
			};

			size_t ret;
			do {
				output.pos = 0;
				ret = ZSTD_endStream(zs->cstream, &output);

				if (output.pos > 0) {
					stream_write(stream->underlying, stream->outbuf,
						     output.pos);
				}
			} while (ret > 0);
		}

		/* Clean up zstd state */
		if (zs->initialized) {
			if (zs->is_writing)
				ZSTD_freeCStream(zs->cstream);
			else
				ZSTD_freeDStream(zs->dstream);
		}

		free(zs);
		break;
	}
#endif

	default:
		break;
	}

	stream->codec_state = NULL;

	/* Free any emulated mmap */
	if (stream->emulated_mmap_addr) {
		free(stream->emulated_mmap_addr);
		stream->emulated_mmap_addr = NULL;
	}

	/* Close underlying stream if we own it */
	if (stream->owns_underlying && stream->underlying)
		stream_close(stream->underlying);

	return 0;
}

/* Auto-detect compression from magic bytes */
int compression_stream_auto(struct compression_stream *stream,
			     struct stream *underlying,
			     int owns_underlying)
{
	unsigned char magic[16];
	ssize_t nread;
	enum compression_type type = COMPRESS_NONE;

	/* Read magic bytes */
	nread = stream_read(underlying, magic, sizeof(magic));
	if (nread < 0)
		return nread;

	/* Seek back */
	if (stream_seek(underlying, 0, SEEK_SET) < 0)
		return -EIO;

	/* Detect compression type */
	if (nread >= 2 && magic[0] == 0x1f && magic[1] == 0x8b) {
		/* gzip: 1f 8b */
		type = COMPRESS_GZIP;
	}
#ifdef STREAM_HAVE_BZIP2
	else if (nread >= 3 && magic[0] == 'B' && magic[1] == 'Z' && magic[2] == 'h') {
		/* bzip2: 42 5a 68 ("BZh") */
		type = COMPRESS_BZIP2;
	}
#endif
#ifdef STREAM_HAVE_LZMA
	else if (nread >= 6 && magic[0] == 0xFD && magic[1] == '7' &&
		 magic[2] == 'z' && magic[3] == 'X' &&
		 magic[4] == 'Z' && magic[5] == 0x00) {
		/* XZ: fd 37 7a 58 5a 00 */
		type = COMPRESS_XZ;
	}
#endif
#ifdef STREAM_HAVE_ZSTD
	else if (nread >= 4 && magic[0] == 0x28 && magic[1] == 0xB5 &&
		 magic[2] == 0x2F && magic[3] == 0xFD) {
		/* Zstd: 28 b5 2f fd */
		type = COMPRESS_ZSTD;
	}
#endif
	else {
		/* Not a supported compressed format */
		return -EINVAL;
	}

	return compression_stream_init(stream, underlying, type, O_RDONLY,
				       owns_underlying);
}

#endif /* STREAM_HAVE_ZLIB || STREAM_HAVE_BZIP2 || STREAM_HAVE_LZMA || STREAM_HAVE_ZSTD */
