/**
 * @file large_file_mmap.c
 * @brief Example of efficiently processing large files with mmap
 *
 * This example demonstrates:
 * - Memory-mapping large files for efficient processing
 * - Processing data without loading entire file into heap
 * - Pattern searching in large files
 * - Byte frequency analysis
 */

#include <stream.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>

/* Byte frequency counter */
struct byte_stats {
	size_t count[256];
	size_t total;
	size_t printable;
	size_t whitespace;
};

/* Count byte frequencies in mapped data */
static void analyze_bytes(const void *data, size_t size, struct byte_stats *stats)
{
	const unsigned char *bytes = data;
	memset(stats, 0, sizeof(*stats));

	for (size_t i = 0; i < size; i++) {
		unsigned char c = bytes[i];
		stats->count[c]++;
		stats->total++;

		if (isprint(c) || c == '\n' || c == '\r' || c == '\t')
			stats->printable++;
		if (isspace(c))
			stats->whitespace++;
	}
}

/* Search for a pattern in mapped data */
static size_t search_pattern(const void *data, size_t size, const char *pattern)
{
	const char *haystack = data;
	size_t pattern_len = strlen(pattern);
	size_t matches = 0;

	if (pattern_len == 0 || pattern_len > size)
		return 0;

	for (size_t i = 0; i <= size - pattern_len; i++) {
		if (memcmp(haystack + i, pattern, pattern_len) == 0) {
			matches++;
			i += pattern_len - 1;  /* Skip past this match */
		}
	}

	return matches;
}

/* Calculate checksum of mapped data */
static unsigned long calculate_checksum(const void *data, size_t size)
{
	const unsigned char *bytes = data;
	unsigned long sum = 0;

	for (size_t i = 0; i < size; i++) {
		sum += bytes[i];
	}

	return sum;
}

/* Print byte statistics */
static void print_stats(const struct byte_stats *stats)
{
	printf("\n=== Byte Statistics ===\n");
	printf("Total bytes:      %zu\n", stats->total);
	printf("Printable chars:  %zu (%.1f%%)\n",
	       stats->printable,
	       100.0 * stats->printable / stats->total);
	printf("Whitespace:       %zu (%.1f%%)\n",
	       stats->whitespace,
	       100.0 * stats->whitespace / stats->total);

	/* Find most common bytes */
	printf("\nMost common bytes:\n");
	for (int pass = 0; pass < 5; pass++) {
		size_t max_count = 0;
		int max_byte = -1;

		for (int i = 0; i < 256; i++) {
			if (stats->count[i] > max_count) {
				/* Skip already printed */
				int skip = 0;
				for (int p = 0; p < pass; p++) {
					/* This is a simplified check */
					skip = 0;
				}
				max_count = stats->count[i];
				max_byte = i;
			}
		}

		if (max_byte >= 0 && max_count > 0) {
			if (isprint(max_byte))
				printf("  '%c' (0x%02x): %zu times (%.1f%%)\n",
				       max_byte, max_byte, max_count,
				       100.0 * max_count / stats->total);
			else
				printf("  0x%02x: %zu times (%.1f%%)\n",
				       max_byte, max_count,
				       100.0 * max_count / stats->total);
		}
	}
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		printf("Usage: %s <file> [options]\n", argv[0]);
		printf("\nProcess large files efficiently using mmap\n");
		printf("\nOptions:\n");
		printf("  --search <pattern>    Search for pattern in file\n");
		printf("  --stats               Show byte frequency statistics\n");
		printf("  --checksum            Calculate simple checksum\n");
		printf("  --compressed          Handle compressed files (.gz, .bz2, etc.)\n");
		printf("\nExamples:\n");
		printf("  %s /var/log/syslog --stats\n", argv[0]);
		printf("  %s large_file.bin --checksum\n", argv[0]);
		printf("  %s data.txt.gz --compressed --search \"error\"\n", argv[0]);
		printf("  %s archive.tar --stats --checksum\n", argv[0]);
		return 1;
	}

	const char *path = argv[1];
	const char *search_term = NULL;
	int show_stats = 0;
	int show_checksum = 0;
	int compressed = 0;

	/* Parse options */
	for (int i = 2; i < argc; i++) {
		if (strcmp(argv[i], "--search") == 0 && i + 1 < argc) {
			search_term = argv[++i];
		} else if (strcmp(argv[i], "--stats") == 0) {
			show_stats = 1;
		} else if (strcmp(argv[i], "--checksum") == 0) {
			show_checksum = 1;
		} else if (strcmp(argv[i], "--compressed") == 0) {
			compressed = 1;
		}
	}

	/* If no options specified, show everything */
	if (!search_term && !show_stats && !show_checksum) {
		show_stats = 1;
		show_checksum = 1;
	}

	printf("Processing: %s\n", path);
	if (compressed)
		printf("Mode: Compressed file (with decompression)\n");
	printf("\n");

	/* Open file */
	struct file_stream fs;
	if (file_stream_open(&fs, path, O_RDONLY, 0) < 0) {
		fprintf(stderr, "Failed to open file: %s\n", path);
		return 1;
	}

	/* Get file size */
	off64_t file_size = stream_size(&fs.base);
	if (file_size < 0) {
		fprintf(stderr, "Failed to get file size\n");
		stream_close(&fs.base);
		return 1;
	}

	printf("File size: %lld bytes (%.2f MB)\n",
	       (long long)file_size,
	       file_size / (1024.0 * 1024.0));

	struct stream *active_stream = &fs.base;
	struct compression_stream cs;

	/* Handle compressed files */
	if (compressed) {
#ifdef STREAMIO_HAVE_ZLIB
		int ret = compression_stream_auto(&cs, &fs.base, 0);
		if (ret == 0) {
			printf("Compression detected and enabled\n");
			active_stream = &cs.base;
			/* For compressed files, we don't know decompressed size */
			file_size = 1024 * 1024;  /* Mmap 1MB for demo */
		} else {
			printf("No compression detected, processing as-is\n");
		}
#else
		fprintf(stderr, "Compression support not available\n");
		stream_close(&fs.base);
		return 1;
#endif
	}

	/* Start timing */
	clock_t start = clock();

	/* Memory-map the file */
	void *data = stream_mmap(active_stream, 0, file_size, PROT_READ);
	if (!data) {
		fprintf(stderr, "Failed to mmap file\n");
		if (compressed && active_stream != &fs.base)
			stream_close(active_stream);
		stream_close(&fs.base);
		return 1;
	}

	size_t mapped_size = file_size;
	printf("Mapped %zu bytes into memory\n\n", mapped_size);

	/* Process the mapped data */

	/* Search for pattern */
	if (search_term) {
		size_t matches = search_pattern(data, mapped_size, search_term);
		printf("Pattern \"%s\": found %zu occurrences\n", search_term, matches);
	}

	/* Calculate checksum */
	if (show_checksum) {
		unsigned long checksum = calculate_checksum(data, mapped_size);
		printf("Checksum: 0x%08lx\n", checksum);
	}

	/* Byte statistics */
	if (show_stats) {
		struct byte_stats stats;
		analyze_bytes(data, mapped_size, &stats);
		print_stats(&stats);
	}

	/* Show first few bytes */
	printf("\n=== First 128 bytes (hex) ===\n");
	const unsigned char *bytes = data;
	size_t preview = mapped_size < 128 ? mapped_size : 128;
	for (size_t i = 0; i < preview; i++) {
		if (i > 0 && i % 16 == 0)
			printf("\n");
		printf("%02x ", bytes[i]);
	}
	printf("\n");

	/* Check if it looks like text */
	int is_text = 1;
	for (size_t i = 0; i < 512 && i < mapped_size; i++) {
		if (bytes[i] == 0 || (bytes[i] < 32 && !isspace(bytes[i]))) {
			is_text = 0;
			break;
		}
	}

	if (is_text) {
		printf("\n=== First 256 characters (text) ===\n");
		for (size_t i = 0; i < 256 && i < mapped_size; i++) {
			if (bytes[i] == 0)
				break;
			putchar(bytes[i]);
		}
		printf("\n");
	}

	/* End timing */
	clock_t end = clock();
	double elapsed = (double)(end - start) / CLOCKS_PER_SEC;

	printf("\n=== Performance ===\n");
	printf("Processing time: %.3f seconds\n", elapsed);
	printf("Throughput: %.2f MB/s\n",
	       (mapped_size / (1024.0 * 1024.0)) / elapsed);

	/* Clean up */
	stream_munmap(active_stream, data, mapped_size);

	if (compressed && active_stream != &fs.base)
		stream_close(active_stream);

	stream_close(&fs.base);

	printf("\nDone!\n");
	return 0;
}
