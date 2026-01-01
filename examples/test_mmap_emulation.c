/**
 * @file test_mmap_emulation.c
 * @brief Test emulated mmap on compressed streams
 */

#include <stream.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/mman.h>
#endif
#include <fcntl.h>

int main(int argc, char **argv)
{
	if (argc < 2) {
		printf("Usage: %s <compressed_file>\n", argv[0]);
		printf("Example: %s file.gz\n", argv[0]);
		return 1;
	}

	const char *path = argv[1];

	/* Open compressed file */
	struct file_stream fs;
	if (file_stream_open(&fs, path, O_RDONLY, 0) < 0) {
		fprintf(stderr, "Failed to open %s\n", path);
		return 1;
	}

	/* Auto-detect and decompress */
	struct compression_stream cs;
	int ret = compression_stream_auto(&cs, &fs.base, 1);
	if (ret < 0) {
		fprintf(stderr, "Failed to initialize decompression: %d\n", ret);
		stream_close(&fs.base);
		return 1;
	}

	printf("Testing emulated mmap on: %s\n", path);
	printf("Stream capabilities: 0x%x\n", stream_get_caps(&cs.base));
	printf("Can mmap: %s\n", stream_can_mmap(&cs.base) ? "yes" : "no");

	/* Try to mmap the entire decompressed content */
	/* For this test, we'll mmap up to 64KB */
	size_t map_size = 65536;
	void *data = stream_mmap(&cs.base, 0, map_size, PROT_READ);
	if (!data) {
		fprintf(stderr, "Failed to mmap stream\n");
		stream_close(&cs.base);
		return 1;
	}

	printf("\nMapped %zu bytes successfully!\n", map_size);
	printf("First 256 bytes (hex):\n");

	unsigned char *bytes = data;
	for (size_t i = 0; i < 256 && i < map_size; i++) {
		if (i > 0 && i % 16 == 0)
			printf("\n");
		printf("%02x ", bytes[i]);
	}
	printf("\n");

	/* Try to show as text if it looks like text */
	int is_text = 1;
	for (size_t i = 0; i < 100 && i < map_size; i++) {
		if (bytes[i] < 32 && bytes[i] != '\n' && bytes[i] != '\r' && bytes[i] != '\t') {
			is_text = 0;
			break;
		}
	}

	if (is_text) {
		printf("\nFirst 256 bytes (text):\n");
		for (size_t i = 0; i < 256 && i < map_size; i++) {
			if (bytes[i] == '\0')
				break;
			putchar(bytes[i]);
		}
		printf("\n");
	}

	/* Unmap */
	ret = stream_munmap(&cs.base, data, map_size);
	if (ret < 0) {
		fprintf(stderr, "Failed to munmap: %d\n", ret);
	} else {
		printf("\nUnmapped successfully!\n");
	}

	/* Clean up */
	stream_close(&cs.base);

	printf("\nTest completed successfully!\n");
	return 0;
}
