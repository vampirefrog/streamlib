#pragma once
#ifdef HAVE_GZIP
#include <zlib.h>
#endif

#include "stream.h"

/**
 * @struct file_stream
 * @brief File stream structure for handling file-based streams.
 */
struct file_stream {
	struct stream stream; /**< Base stream structure */
#ifdef HAVE_GZIP
	union {
#endif
		FILE *f;
#ifdef HAVE_GZIP
		gzFile gz;
	};
#endif
};

/**
 * @brief Initialize a file stream.
 * @param stream Pointer to the file stream object.
 * @param filename Name of the file to open.
 * @param mode Mode in which to open the file.
 * @return Status code.
 */
int file_stream_init(struct file_stream *stream, const char *filename, const char *mode, int stream_flags);

#ifdef WIN32
/**
 * @brief Initialize a file stream with wide-character filename.
 * @param stream Pointer to the file stream object.
 * @param filename Wide-character name of the file to open.
 * @param mode Wide-character mode in which to open the file.
 * @return Status code.
 */
int file_stream_initw(struct file_stream *stream, const wchar_t *filename, const wchar_t *mode, int stream_flags);
#endif

/**
 * @brief Create a file stream.
 * @param filename Name of the file to open.
 * @param modeHere is the continuation and completion of the Doxygen comments for `stream.h`:
 * @param mode Mode in which to open the file.
 * @return Pointer to the created file stream object.
 */
struct stream *file_stream_new(const char *filename, const char *mode, int stream_flags);

#ifdef WIN32
/**
 * @brief Create a file stream with wide-character filename.
 * @param filename Wide-character name of the file to open.
 * @param mode Wide-character mode in which to open the file.
 * @return Pointer to the created file stream object.
 */
struct stream *file_stream_neww(const wchar_t *filename, const wchar_t *mode, int stream_flags);
#endif
