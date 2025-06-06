#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <zip.h>
#include <zlib.h>
#include <stdlib.h>
#include "../stream.h"

// Memory Stream Tests
void test_mem_stream_init() {
	struct mem_stream mstream;
	assert(mem_stream_init(&mstream, 0, 0, 0) == 0);
	assert(stream_close((struct stream *)&mstream) == 0);
}

void test_mem_stream_write_read() {
	struct mem_stream mstream;
	mem_stream_init(&mstream, 0, 0, 0);

	const char *data = "Hello, StreamLib!";
	assert(stream_write((struct stream *)&mstream, data, strlen(data)) == (ssize_t)strlen(data));

	char buffer[20];
	assert(stream_seek((struct stream *)&mstream, 0, SEEK_SET) == 0);
	assert(stream_read((struct stream *)&mstream, buffer, strlen(data)) == (ssize_t)strlen(data));
	buffer[strlen(data)] = '\0';

	assert(strcmp(buffer, data) == 0);

	assert(stream_close((struct stream *)&mstream) == 0);

	char data_gz[] = {
		0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x03, 0xf3, 0x48, 0xcd, 0xc9, 0xc9, 0xd7,
		0x51, 0x08, 0x2e, 0x29, 0x4a, 0x4d, 0xcc, 0xf5,
		0xc9, 0x4c, 0x52, 0x04, 0x00, 0xef, 0x54, 0x9d,
		0xc5, 0x11, 0x00, 0x00, 0x00
	};
	mem_stream_init(&mstream, data_gz, sizeof(data_gz), STREAM_TRANSPARENT_GZIP);
	assert(stream_read((struct stream *)&mstream, buffer, strlen(data)) == (ssize_t)strlen(data));
	buffer[strlen(data)] = '\0';
	assert(strcmp(buffer, data) == 0);
}

// File Stream Tests
void test_file_stream_init() {
	struct file_stream fstream;
	assert(file_stream_init(&fstream, "test.txt", "w", 0) == 0);
	assert(stream_close((struct stream *)&fstream) == 0);
}

void test_file_stream_write_read() {
	struct file_stream fstream;
	file_stream_init(&fstream, "test.txt", "w+", 0);

	const char *data = "Hello, StreamLib!";
	assert(stream_write((struct stream *)&fstream, data, strlen(data)) == (ssize_t)strlen(data));

	char buffer[20];
	assert(stream_seek((struct stream *)&fstream, 0, SEEK_SET) == 0);
	assert(stream_read((struct stream *)&fstream, buffer, strlen(data)) == (ssize_t)strlen(data));
	buffer[strlen(data)] = '\0';

	assert(strcmp(buffer, data) == 0);

	assert(stream_close((struct stream *)&fstream) == 0);
}

void test_file_stream_mmap() {
    const char *data = "Hello, mmap!";
    size_t data_len = strlen(data);

    // Write data to file
    struct file_stream fstream;
    assert(file_stream_init(&fstream, "mmap_test.txt", "w+", 0) == 0);
    assert(stream_write((struct stream *)&fstream, data, data_len) == (ssize_t)data_len);
    assert(stream_close((struct stream *)&fstream) == 0);

    // Open file for reading and mmap
    assert(file_stream_init(&fstream, "mmap_test.txt", "r", 0) == 0);

    void *mapped = NULL;
    size_t mapped_len = 0;
    mapped = stream_get_memory_access((struct stream *)&fstream, &mapped_len);
    assert(mapped != NULL);
    assert(mapped_len == data_len);
    assert(memcmp(mapped, data, data_len) == 0);

    assert(stream_revoke_memory_access((struct stream *)&fstream) == 0);
    assert(stream_close((struct stream *)&fstream) == 0);

    remove("mmap_test.txt");
}

// Helper: gzip-compress a buffer
size_t gzip_compress(const char *input, size_t input_len, char **output) {
    z_stream zs = {0};
    deflateInit2(&zs, Z_BEST_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY);

    size_t out_size = input_len + 64;
    *output = malloc(out_size);
    zs.next_in = (Bytef *)input;
    zs.avail_in = input_len;
    zs.next_out = (Bytef *)*output;
    zs.avail_out = out_size;

    int ret = deflate(&zs, Z_FINISH);
    deflateEnd(&zs);

    if (ret != Z_STREAM_END) {
        free(*output);
        *output = NULL;
        return 0;
    }
    return zs.total_out;
}

// Test: read gzip file inside zip archive
void test_zip_gzip_stream_read() {
    const char *data = "Hello, StreamLib!";
    char *gz_data = NULL;
    size_t gz_len = gzip_compress(data, strlen(data), &gz_data);
    assert(gz_len > 0);

    // Create zip archive with gzip file
    int errorp;
    zip_t *za = zip_open("test.zip", ZIP_CREATE | ZIP_TRUNCATE, &errorp);
    assert(za);

    zip_source_t *zs = zip_source_buffer(za, gz_data, gz_len, 0);
    assert(zs);
    assert(zip_file_add(za, "test.txt.gz", zs, ZIP_FL_OVERWRITE) >= 0);
    assert(zip_close(za) == 0);

    free(gz_data);

    // Now open the file inside the zip with transparent gzip
    struct zip_file_stream zstream;
	zip_t *zip_archive = zip_open("test.zip", ZIP_RDONLY, NULL);
	assert(zip_archive != NULL);
	assert(zip_file_stream_init_index(&zstream, zip_archive, 0, STREAM_TRANSPARENT_GZIP) == 0);

    char buffer[64];
    ssize_t n = stream_read((struct stream *)&zstream, buffer, strlen(data));
    assert(n == (ssize_t)strlen(data));
    buffer[n] = '\0';
    assert(strcmp(buffer, data) == 0);

    assert(stream_close((struct stream *)&zstream) == 0);

    remove("test.zip");
}

// Main function to run all tests
int main() {
	// Memory Stream Tests
	test_mem_stream_init();
	test_mem_stream_write_read();

	// File Stream Tests
	test_file_stream_init();
	test_file_stream_write_read();
	test_file_stream_mmap();

    // Zip Gzip Stream Test
    test_zip_gzip_stream_read();
    
	printf("All tests passed!\n");
	return 0;
}
