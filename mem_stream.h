#pragma once

#include "stream.h"

/**
 * @struct mem_stream
 * @brief Memory stream structure for handling in-memory data streams.
 */
struct mem_stream {
	struct stream stream; /**< Base stream structure */
	void *data; /**< Pointer to the data buffer */
	size_t data_len; /**< Length of the data */
	ssize_t allocated_len; /**< Allocated length of the data buffer, -1 if using user buffer */
	size_t position; /**< Current position in the stream */
};

/**
 * @brief Initialize a memory stream.
 * @param stream Pointer to the memory stream object.
 * @param existing_data Pointer to the existing data buffer. If NULL, data_len bytes are allocated.
 * @param data_len Length of the data.
 * @return Status code.
 */
int mem_stream_init(struct mem_stream *stream, void *existing_data, size_t data_len, int stream_flags);

/**
 * @brief Create a memory stream.
 * @param existing_data Pointer to the existing data buffer. If NULL, data_len bytes are allocated.
 * @param existing_data_len Length of the data.
 * @return Pointer to the created memory stream object.
 */
struct stream *mem_stream_new(void *existing_data, size_t existing_data_len, int stream_flags);
