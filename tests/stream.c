#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../stream.h"

// Memory Stream Tests
void test_mem_stream_init() {
	struct mem_stream mstream;
	assert(mem_stream_init(&mstream, 0, 0) == 0);
	assert(stream_close((struct stream *)&mstream) == 0);
}

void test_mem_stream_write_read() {
	struct mem_stream mstream;
	mem_stream_init(&mstream, 0, 0);

	const char *data = "Hello, StreamLib!";
	assert(stream_write((struct stream *)&mstream, data, strlen(data)) == (ssize_t)strlen(data));

	char buffer[20];
	assert(stream_seek((struct stream *)&mstream, 0, SEEK_SET) == 0);
	assert(stream_read((struct stream *)&mstream, buffer, strlen(data)) == (ssize_t)strlen(data));
	buffer[strlen(data)] = '\0';

	assert(strcmp(buffer, data) == 0);

	assert(stream_close((struct stream *)&mstream) == 0);
}

// File Stream Tests
void test_file_stream_init() {
	struct file_stream fstream;
	assert(file_stream_init(&fstream, "test.txt", MODE_WRITE) == 0);
	assert(stream_close((struct stream *)&fstream) == 0);
}

void test_file_stream_write_read() {
	struct file_stream fstream;
	file_stream_init(&fstream, "test.txt", MODE_WRITE | MODE_READ);

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
