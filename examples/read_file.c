/**
 * @file read_file.c
 * @brief Simple example showing how to read a file using streamio
 */

#include <stream.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <file>\n", argv[0]);
		return 1;
	}

	const char *filename = argv[1];

	/* Print library info */
	printf("StreamIO version: %s\n", stream_get_version());
	printf("Features: %s\n\n", stream_get_features_string());

	/* Open file */
	struct file_stream fs;
	int ret = file_stream_open(&fs, filename, O_RDONLY, 0);
	if (ret < 0) {
		fprintf(stderr, "Failed to open file '%s': %s\n",
			filename, strerror(-ret));
		return 1;
	}

	/* Get file size */
	off64_t size = stream_size(&fs.base);
	if (size < 0) {
		fprintf(stderr, "Failed to get file size: %s\n",
			strerror(-size));
		stream_close(&fs.base);
		return 1;
	}

	printf("File: %s\n", filename);
	printf("Size: %lld bytes\n", (long long)size);

	/* Check capabilities */
	printf("\nCapabilities:\n");
	printf("  Can read: %s\n", stream_can_read(&fs.base) ? "yes" : "no");
	printf("  Can write: %s\n", stream_can_write(&fs.base) ? "yes" : "no");
	printf("  Can seek: %s\n", stream_can_seek(&fs.base) ? "yes" : "no");
	printf("  Can mmap: %s\n", stream_can_mmap(&fs.base) ? "yes" : "no");

	/* Read and display first 256 bytes */
	printf("\nFirst 256 bytes (or entire file if smaller):\n");
	printf("-------------------------------------------\n");

	char buf[257];
	size_t to_read = (size < 256) ? size : 256;
	ssize_t nread = stream_read(&fs.base, buf, to_read);

	if (nread < 0) {
		fprintf(stderr, "Read failed: %s\n", strerror(-nread));
		stream_close(&fs.base);
		return 1;
	}

	buf[nread] = '\0';
	printf("%s", buf);

	if (nread < size)
		printf("\n... (%lld more bytes)\n", (long long)(size - nread));

	printf("\n");

	/* Close stream */
	stream_close(&fs.base);

	return 0;
}
