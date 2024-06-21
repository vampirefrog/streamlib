#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../stream.h"

// Memory Stream Tests
void test_mem_stream_init() {
	struct mem_stream mstream;
	assert(mem_stream_init(&mstream, 0, 0) == 0);
	stream_close((struct stream *)&mstream);
}

void test_mem_stream_write_read() {
	struct mem_stream mstream;
	mem_stream_init(&mstream, 0, 0);

	const char *data = "Hello, StreamLib!";
	stream_write((struct stream *)&mstream, data, strlen(data));

	char buffer[20];
	stream_seek((struct stream *)&mstream, 0, SEEK_SET);
	stream_read((struct stream *)&mstream, buffer, strlen(data));
	buffer[strlen(data)] = '\0';

	assert(strcmp(buffer, data) == 0);

	stream_close((struct stream *)&mstream);
}

// File Stream Tests
void test_file_stream_init() {
	struct file_stream fstream;
	assert(file_stream_init(&fstream, "test.txt", "w") == 0);
	stream_close((struct stream *)&fstream);
}

void test_file_stream_write_read() {
	struct file_stream fstream;
	file_stream_init(&fstream, "test.txt", "w+");

	const char *data = "Hello, StreamLib!";
	stream_write((struct stream *)&fstream, data, strlen(data));

	char buffer[20];
	stream_seek((struct stream *)&fstream, 0, SEEK_SET);
	stream_read((struct stream *)&fstream, buffer, strlen(data));
	buffer[strlen(data)] = '\0';

	assert(strcmp(buffer, data) == 0);

	stream_close((struct stream *)&fstream);
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
