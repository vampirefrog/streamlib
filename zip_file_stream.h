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
#endif
