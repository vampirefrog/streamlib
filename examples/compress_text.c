/**
 * @file compress_text.c
 * @brief Example comparing compression ratios across different formats
 */

#include <stream.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#ifndef HAVE_COMPRESSION
int main(void)
{
	fprintf(stderr, "Error: This library was built without compression support\n");
	fprintf(stderr, "Rebuild with at least one of: -DENABLE_ZLIB=ON, -DENABLE_BZIP2=ON, -DENABLE_LZMA=ON, -DENABLE_ZSTD=ON\n");
	return 1;
}
#else

/* Default sample text (repetitive for better compression) */
static const char *default_text =
	"Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
	"Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. "
	"Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris. "
	"Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
	"Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. "
	"Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris. "
	"Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
	"Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. "
	"Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris. ";

/* Structure to hold compression results */
struct compress_result {
	const char *format_name;
	enum compression_type type;
	const char *extension;
	int available;
	off64_t compressed_size;
	double ratio;
};

/* Get file size */
static off64_t get_file_size(const char *filename)
{
	struct stat st;
	if (stat(filename, &st) < 0)
		return -1;
	return st.st_size;
}

/* Compress text to a specific format */
static int compress_to_format(const char *text, size_t text_len,
			       enum compression_type type,
			       const char *output_file,
			       off64_t *output_size)
{
	/* Open output file */
	struct file_stream output_fs;
	int ret = file_stream_open(&output_fs, output_file,
				   O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (ret < 0) {
		fprintf(stderr, "Error: Failed to open output file '%s': %s\n",
			output_file, strerror(-ret));
		return ret;
	}

	/* Create compression stream */
	struct compression_stream cs;
	ret = compression_stream_init(&cs, &output_fs.base, type, O_WRONLY, 1);
	if (ret < 0) {
		fprintf(stderr, "Error: Failed to initialize compression: %s\n",
			strerror(-ret));
		stream_close(&output_fs.base);
		return ret;
	}

	/* Write compressed data */
	ssize_t written = stream_write(&cs.base, text, text_len);
	if (written < 0) {
		fprintf(stderr, "Error: Failed to write compressed data: %s\n",
			strerror(-written));
		stream_close(&cs.base);
		return written;
	}

	/* Close stream (finalizes compression) */
	stream_close(&cs.base);  /* Also closes output_fs */

	/* Get compressed file size */
	*output_size = get_file_size(output_file);
	if (*output_size < 0) {
		fprintf(stderr, "Warning: Could not stat output file\n");
		return -1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	const char *text;
	size_t text_len;

	/* Print library info */
	printf("StreamIO Compression Comparison\n");
	printf("================================\n\n");
	printf("Version: %s\n", stream_get_version());
	printf("Features: %s\n\n", stream_get_features_string());

	/* Get input text */
	if (argc > 1) {
		/* Use provided text */
		text = argv[1];
		text_len = strlen(text);
		printf("Input: Custom text (%zu bytes)\n\n", text_len);
	} else {
		/* Use default sample text, repeated multiple times */
		static char buffer[65536];
		size_t offset = 0;
		size_t sample_len = strlen(default_text);

		/* Repeat sample text to fill buffer */
		while (offset + sample_len < sizeof(buffer)) {
			memcpy(buffer + offset, default_text, sample_len);
			offset += sample_len;
		}
		buffer[offset] = '\0';

		text = buffer;
		text_len = offset;
		printf("Input: Sample text (repeated, %zu bytes)\n\n", text_len);
	}

	/* Define formats to test */
	struct compress_result results[] = {
		{"gzip",  COMPRESS_GZIP,  ".gz",  0, 0, 0.0},
		{"bzip2", COMPRESS_BZIP2, ".bz2", 0, 0, 0.0},
		{"xz",    COMPRESS_XZ,    ".xz",  0, 0, 0.0},
		{"zstd",  COMPRESS_ZSTD,  ".zst", 0, 0, 0.0},
	};
	int num_formats = sizeof(results) / sizeof(results[0]);

	/* Test each format */
	printf("Compressing to each format...\n");
	for (int i = 0; i < num_formats; i++) {
		results[i].available = compression_is_available(results[i].type);

		if (!results[i].available) {
			printf("  %s: Not available\n", results[i].format_name);
			continue;
		}

		/* Create output filename */
		char output_file[256];
		snprintf(output_file, sizeof(output_file),
			 "/tmp/compress_test%s", results[i].extension);

		/* Compress */
		printf("  %s: ", results[i].format_name);
		fflush(stdout);

		int ret = compress_to_format(text, text_len, results[i].type,
					      output_file,
					      &results[i].compressed_size);
		if (ret < 0) {
			printf("FAILED\n");
			continue;
		}

		/* Calculate compression ratio */
		results[i].ratio = (double)results[i].compressed_size /
				   (double)text_len * 100.0;

		printf("%lld bytes (%.1f%%)\n",
		       (long long)results[i].compressed_size,
		       results[i].ratio);

		/* Clean up */
		unlink(output_file);
	}

	/* Print comparison table */
	printf("\n");
	printf("Compression Results\n");
	printf("===================\n\n");
	printf("Original size: %zu bytes\n\n", text_len);

	printf("%-10s %-12s %-15s %-10s\n",
	       "Format", "Size", "Ratio", "Saved");
	printf("%-10s %-12s %-15s %-10s\n",
	       "----------", "------------", "---------------", "----------");

	for (int i = 0; i < num_formats; i++) {
		if (!results[i].available) {
			printf("%-10s %-12s %-15s %-10s\n",
			       results[i].format_name,
			       "N/A", "N/A", "N/A");
			continue;
		}

		if (results[i].compressed_size == 0) {
			printf("%-10s %-12s %-15s %-10s\n",
			       results[i].format_name,
			       "FAILED", "FAILED", "FAILED");
			continue;
		}

		off64_t saved = text_len - results[i].compressed_size;
		double saved_pct = (double)saved / (double)text_len * 100.0;

		char size_str[32], ratio_str[32], saved_str[32];
		snprintf(size_str, sizeof(size_str), "%lld bytes",
			 (long long)results[i].compressed_size);
		snprintf(ratio_str, sizeof(ratio_str), "%.1f%%",
			 results[i].ratio);
		snprintf(saved_str, sizeof(saved_str), "%lld (%.1f%%)",
			 (long long)saved, saved_pct);

		printf("%-10s %-12s %-15s %-10s\n",
		       results[i].format_name,
		       size_str, ratio_str, saved_str);
	}

	/* Find best compression */
	printf("\n");
	int best_idx = -1;
	off64_t best_size = text_len;

	for (int i = 0; i < num_formats; i++) {
		if (results[i].available && results[i].compressed_size > 0 &&
		    results[i].compressed_size < best_size) {
			best_size = results[i].compressed_size;
			best_idx = i;
		}
	}

	if (best_idx >= 0) {
		printf("Best compression: %s (%.1f%% of original)\n",
		       results[best_idx].format_name,
		       results[best_idx].ratio);
	}

	printf("\n");
	printf("Note: Compression effectiveness varies by data type.\n");
	printf("      Text compresses better than random/binary data.\n");

	return 0;
}
#endif
