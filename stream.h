#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#ifdef HAVE_LIBZIP
#include <zip.h>
#endif

struct stream {
	size_t position;
	int _errno;

	size_t (*read)(struct stream *, void *ptr, size_t size);
	size_t (*write)(struct stream *, void *ptr, size_t size);
	size_t (*seek)(struct stream *, long offset, int whence);
	int (*eof)(struct stream *);
	long (*tell)(struct stream *);
	int (*vprintf)(struct stream *, const char *fmt, va_list ap);
	void *(*get_memory_access)(struct stream *, size_t *length);
	void (*revoke_memory_access)(struct stream *);
	int (*close)(struct stream *);
};

size_t stream_read(struct stream *, void *ptr, size_t size);
ssize_t stream_write(struct stream *stream, void *ptr, size_t size);
size_t stream_seek(struct stream *stream, long offset, int whence);
int stream_eof(struct stream *stream);
long stream_tell(struct stream *stream);
int stream_printf(struct stream *stream, const char *fmt, ...);
void *stream_get_memory_access(struct stream *stream, size_t *length); /* For plain files, try mmap(), for memory streams, return the pointer to the data, and for zipped and gzipped files, allocate a buffer and read+uncompress the whole file in the buffer */
int stream_revoke_memory_access(struct stream *stream);
int stream_close(struct stream *stream); // deinitialize the stream: calls fclose() for file streams, and frees the memory buffer for memory streams
int stream_destroy(struct stream *stream); // deinitialize (see stream_close()) and free the stream structure

/* Convenience functions */
uint8_t stream_read_uint8(struct stream *stream);
uint16_t stream_read_big_uint16(struct stream *stream);
uint32_t stream_read_big_uint32(struct stream *stream);
ssize_t stream_write_uint8(struct stream *stream, uint8_t i);
ssize_t stream_write_big_uint16(struct stream *stream, uint16_t i);
ssize_t stream_write_big_uint32(struct stream *stream, uint32_t i);
int stream_read_compare(struct stream *stream, const void *data, int len);

struct mem_stream {
	struct stream stream;
	void *data;
	int data_len;
	int allocated_len; // -1 if there is no allocation (using user buffer)
	int position;
};
/**
 * @param existing_data
 *   If specified as NULL, data_len bytes are allocated.
 *   Otherwise it uses the existing user provided buffer.
 */
int mem_stream_init(struct mem_stream *stream, void *existing_data, int data_len);
struct stream *mem_stream_create();

struct file_stream {
	struct stream stream;
	FILE *f;
};
int file_stream_init(struct file_stream *stream, char *filename, const char *mode);
struct stream *file_stream_create(char *filename, const char *mode);

#ifdef HAVE_LIBZIP
struct zip_file_stream {
	struct stream stream;
	zip_file_t *f;
	zip_stat_t stat;
	void *mem;
};
int zip_file_stream_init_index(struct zip_file_stream *stream, zip_t *zip, int index);
struct stream *zip_file_stream_create_index(zip_t *zip, int index);
#endif

struct file_type_filter {
	const char *ext;
	int (*file_cb)(const char *full_path, struct stream *stream, void *user_data); /* The stream * pointer is non null if the EF_OPEN_STREAM flag is set */
	void *user_data;
};

#define EF_RECURSE_DIRS 0x01
#ifdef HAVE_LIBZIP
#define EF_RECURSE_ARCHIVES 0x02 /* Currently zip only */
#endif
#define EF_OPEN_STREAM 0x04 /* Open the file and provide the instance */
#define EF_CREATE_WRITABLE_ZIP_DIRS 0x08 /* Create a writable directory with the basename of the zip file before calling callback */
int each_file(const char *path, struct file_type_filter *filters, int flags);

// TODO: add change_file_extension string function
// TODO: proper error handling
