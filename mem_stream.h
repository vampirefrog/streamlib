#pragma once

#ifdef HAVE_GZIP
#include <zlib.h>
#endif

#include "stream_base.h"

struct mem_stream {
	struct stream stream; /**< Base stream structure */
	void *data; /**< Pointer to the data buffer */
	size_t data_len; /**< Length of the data */
	ssize_t allocated_len; /**< Allocated length of the data buffer, -1 if using user buffer */
	size_t position; /**< Current position in the stream */
#ifdef HAVE_GZIP
	z_stream z_stream;
	size_t decompressed_data_len;
#endif
};

#define MEMFS_OK 0
#define MEMFS_ERR_MALLOC    -1
#define MEMFS_ERR_RESIZE    -2
#define MEMFS_ERR_ZLIB_INIT -3
#define MEMFS_ERR_ZLIB_DECOMP -4
#define MEMFS_ERR_UNKNOWN   -100

int mem_stream_init(struct mem_stream *stream, void *existing_data, size_t data_len, int stream_flags);
struct stream *mem_stream_new(void *existing_data, size_t existing_data_len, int stream_flags);
const char *mem_stream_strerror(int err);
