/**
 * @file stream.h
 * @brief Header file for stream operations including memory streams, file streams, and zip file streams.
 */

/**
 * \mainpage StreamLib Documentation
 *
 * \section intro_sec Introduction
 *
 * StreamLib is a versatile C library designed for handling various types of streams.
 * It provides functionality for reading from and writing to memory streams, file streams,
 * and zip file streams. This library aims to simplify stream management and provide
 * utility functions for common operations.
 *
 * \section features_sec Features
 *
 * - Support for memory streams, file streams, and zip file streams.
 * - Reading, writing, seeking, and other stream operations.
 * - Convenient functions for handling different stream types.
 *
 * \section usage_sec Usage
 *
 * To use StreamLib in your project, include the appropriate headers and link with the
 * StreamLib library. Refer to the API documentation for detailed function references
 * and examples.
 *
 * \section license_sec License
 *
 * StreamLib is licensed under the MIT License. See the LICENSE file for more details.
 *
 * \section github_sec GitHub Repository
 *
 * Visit the <a href="https://github.com/vampirefrog/streamlib">StreamLib GitHub repository</a>
 * for more information, contributions, and issues.
 */

#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#ifdef HAVE_LIBZIP
#include <zip.h>
#endif

/**
 * @struct stream
 * @brief Generic stream structure to handle various stream operations.
 */
struct stream {
	int _errno; /**< Error number */
	void *mem; /**< Pointer for memory access functions */
	size_t mem_size; /**< Size of the memory buffer */

	ssize_t (*read)(struct stream *, void *ptr, size_t size); /**< Function pointer to read data from the stream */
	ssize_t (*write)(struct stream *, const void *ptr, size_t size); /**< Function pointer to write data to the stream */
	size_t (*seek)(struct stream *, long offset, int whence); /**< Function pointer to seek within the stream */
	int (*eof)(struct stream *); /**< Function pointer to check for end-of-file in the stream */
	long (*tell)(struct stream *); /**< Function pointer to tell the current position in the stream */
	int (*vprintf)(struct stream *, const char *fmt, va_list ap); /**< Function pointer to formatted print to the stream */
	void *(*get_memory_access)(struct stream *, size_t *length); /**< Function pointer to get memory access */
	int (*revoke_memory_access)(struct stream *); /**< Function pointer to revoke memory access */
	int (*close)(struct stream *); /**< Function pointer to close the stream */
};

void stream_init(struct stream *stream);

/**
 * @brief Read data from the stream.
 * @param stream Pointer to the stream object.
 * @param ptr Pointer to the buffer where data will be read into.
 * @param size Number of bytes to read.
 * @return Number of bytes read.
 */
ssize_t stream_read(struct stream *, void *ptr, size_t size);

/**
 * @brief Write data to the stream.
 * @param stream Pointer to the stream object.
 * @param ptr Pointer to the buffer containing data to write.
 * @param size Number of bytes to write.
 * @return Number of bytes written.
 */
ssize_t stream_write(struct stream *stream, const void *ptr, size_t size);

/**
 * @brief Seek to a specific position in the stream.
 * @param stream Pointer to the stream object.
 * @param offset Offset to seek to.
 * @param whence Position from where offset is added.
 * @return New position in the stream.
 */
size_t stream_seek(struct stream *stream, long offset, int whence);

/**
 * @brief Check if end-of-file is reached in the stream.
 * @param stream Pointer to the stream object.
 * @return Non-zero if end-of-file is reached, zero otherwise.
 */
int stream_eof(struct stream *stream);

/**
 * @brief Get the current position in the stream.
 * @param stream Pointer to the stream object.
 * @return Current position in the stream.
 */
long stream_tell(struct stream *stream);

/**
 * @brief Print formatted data to the stream.
 * @param stream Pointer to the stream object.
 * @param fmt Format string.
 * @param ... Additional arguments for the format string.
 * @return Number of characters printed.
 */
int stream_printf(struct stream *stream, const char *fmt, ...);

/**
 * @brief Get memory access for the stream.
 * @param stream Pointer to the stream object.
 * @param length Pointer to store the length of the memory.
 * @return Pointer to the memory.
 */
void *stream_get_memory_access(struct stream *stream, size_t *length);

/**
 * @brief Revoke memory access for the stream.
 * @param stream Pointer to the stream object.
 * @return Status code.
 */
int stream_revoke_memory_access(struct stream *stream);

/**
 * @brief Close the stream.
 * @param stream Pointer to the stream object.
 * @return Status code.
 */
int stream_close(struct stream *stream);

/**
 * @brief Deinitialize and free the stream structure.
 * @param stream Pointer to the stream object.
 * @return Status code.
 */
int stream_destroy(struct stream *stream);

/**
 * @brief Read an unsigned 8-bit integer from the stream.
 * @param stream Pointer to the stream object.
 * @return The unsigned 8-bit integer read from the stream.
 */
uint8_t stream_read_uint8(struct stream *stream);

/**
 * @brief Read a big-endian unsigned 16-bit integer from the stream.
 * @param stream Pointer to the stream object.
 * @return The big-endian unsigned 16-bit integer read from the stream.
 */
uint16_t stream_read_big_uint16(struct stream *stream);

/**
 * @brief Read a big-endian unsigned 32-bit integer from the stream.
 * @param stream Pointer to the stream object.
 * @return The big-endian unsigned 32-bit integer read from the stream.
 */
uint32_t stream_read_big_uint32(struct stream *stream);

/**
 * @brief Write an unsigned 8-bit integer to the stream.
 * @param stream Pointer to the stream object.
 * @param i The unsigned 8-bit integer to write.
 * @return Number of bytes written.
 */
ssize_t stream_write_uint8(struct stream *stream, uint8_t i);

/**
 * @brief Write a big-endian unsigned 16-bit integer to the stream.
 * @param stream Pointer to the stream object.
 * @param i The big-endian unsigned 16-bit integer to write.
 * @return Number of bytes written.
 */
ssize_t stream_write_big_uint16(struct stream *stream, uint16_t i);

/**
 * @brief Write a big-endian unsigned 32-bit integer to the stream.
 * @param stream Pointer to the stream object.
 * @param i The big-endian unsigned 32-bit integer to write.
 * @return Number of bytes written.
 */
ssize_t stream_write_big_uint32(struct stream *stream, uint32_t i);

/**
 * @brief Compare data read from the stream with provided data.
 * @param stream Pointer to the stream object.
 * @param data Pointer to the data to compare.
 * @param len Length of the data to compare.
 * @return Result of the comparison.
 */
int stream_read_compare(struct stream *stream, const void *data, size_t len);

#include "file_stream.h"
#include "mem_stream.h"
#include "zip_file_stream.h"
#include "each_file.h"

// TODO: proper error handling
