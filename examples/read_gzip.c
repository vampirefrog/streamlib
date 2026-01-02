/**
 * @file read_gzip.c
 * @brief Example showing how to read gzip-compressed files
 */

#include <stream.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <file.gz>\n", argv[0]);
		return 1;
	}

	const char *filename = argv[1];

	/* Print library info */
	printf("StreamIO version: %s\n", stream_get_version());
	printf("Features: %s\n\n", stream_get_features_string());

#ifndef HAVE_ZLIB
	fprintf(stderr, "Error: This library was built without zlib support\n");
	fprintf(stderr, "Rebuild with -DENABLE_ZLIB=ON\n");
	return 1;
#else

	/* Check if gzip is available */
	if (!compression_is_available(COMPRESS_GZIP)) {
		fprintf(stderr, "Error: GZIP compression not available\n");
		return 1;
	}

	/* Open compressed file */
	struct file_stream fs;
	int ret = file_stream_open(&fs, filename, O_RDONLY, 0);
	if (ret < 0) {
		fprintf(stderr, "Failed to open file '%s': %s\n",
			filename, strerror(-ret));
		return 1;
	}

	/* Create gzip decompression stream */
	struct compression_stream cs;
	ret = gzip_stream_init(&cs, &fs.base, O_RDONLY, 1);
	if (ret < 0) {
		fprintf(stderr, "Failed to init gzip stream: %s\n",
			strerror(-ret));
		stream_close(&fs.base);
		return 1;
	}

	printf("Reading compressed file: %s\n", filename);
	printf("-------------------------------------------\n");

	/* Read and display decompressed data */
	char buf[4096];
	ssize_t total_read = 0;
	ssize_t nread;

	while ((nread = stream_read(&cs.base, buf, sizeof(buf) - 1)) > 0) {
		buf[nread] = '\0';
		printf("%s", buf);
		total_read += nread;
	}

	if (nread < 0) {
		fprintf(stderr, "\nError reading compressed data: %s\n",
			strerror(-nread));
		stream_close(&cs.base);
		return 1;
	}

	printf("\n-------------------------------------------\n");
	printf("Total decompressed: %zd bytes\n", total_read);

	/* Close streams */
	stream_close(&cs.base);

	return 0;
#endif
}
