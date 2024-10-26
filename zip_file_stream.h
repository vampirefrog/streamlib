#pragma once

#include "stream.h"

#ifdef HAVE_LIBZIP
/**
 * @struct zip_file_stream
 * @brief Zip file stream structure for handling streams within zip archives.
 */
struct zip_file_stream {
	struct stream stream; /**< Base stream structure */
	zip_file_t *f; /**< Zip file pointer */
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
