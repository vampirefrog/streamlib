#include <assert.h>
#include <stdio.h>
#include <string.h>
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

// Main function to run all tests
int main() {
	// Memory Stream Tests
	test_mem_stream_init();
	test_mem_stream_write_read();

	// File Stream Tests
	test_file_stream_init();
	test_file_stream_write_read();

	printf("All tests passed!\n");
	return 0;
}
