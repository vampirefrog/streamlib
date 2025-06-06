#pragma once

#ifdef HAVE_GZIP
#include <zlib.h>
#endif

#include "stream_base.h"

#ifdef HAVE_LIBZIP
/**
 * @struct zip_file_stream
 * @brief Zip file stream structure for handling streams within zip archives.
 */
struct zip_file_stream {
	struct stream stream; /**< Base stream structure */
	zip_file_t *f; /**< Zip file pointer */
#ifdef HAVE_GZIP
	void *z_data;
	size_t z_position;
	z_stream z_stream;
	size_t decompressed_data_len;
#endif
	void *mem_ptr; /**< Pointer to memory buffer for STREAM_ENSURE_MMAP */
	size_t mem_length; /**< Length of the memory buffer for STREAM_ENSURE_MMAP */
	size_t mem_offset; /**< Offset for memory-backed reads */
	zip_stat_t stat; /**< Zip file statistics */
};

/**
 * @brief Initialize a zip file stream by index.
 * @param stream Pointer to the zip file stream object.
 * @param zip Pointer to the zip archive.
 * @param index Index of the file within the zip archive.
 * @return Status code.
 */
int zip_file_stream_init_index(struct zip_file_stream *stream, zip_t *zip, int index, int stream_flags);

/**
 * @brief Create a zip file stream by index.
 * @param zip Pointer to the zip archive.
 * @param index Index of the file within the zip archive.
 * @return Pointer to the created zip file stream object.
 */
struct stream *zip_file_stream_create_index(zip_t *zip, int index, int stream_flags);

// Error codes for zip_file_stream
#define ZIPFS_OK 0
#define ZIPFS_ERR_STAT         -1
#define ZIPFS_ERR_OPEN         -2
#define ZIPFS_ERR_MALLOC       -3
#define ZIPFS_ERR_READ         -4
#define ZIPFS_ERR_NOT_GZIP     -5
#define ZIPFS_ERR_ZLIB_INIT    -6
#define ZIPFS_ERR_ZLIB_DECOMP  -7
#define ZIPFS_ERR_MMAP         -8
#define ZIPFS_ERR_UNKNOWN      -100
#endif
