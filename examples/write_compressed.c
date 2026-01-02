/**
 * @file write_compressed.c
 * @brief Example showing how to write compressed files in any format
 */

#include <stream.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef HAVE_COMPRESSION
int main(void)
{
	fprintf(stderr, "Error: This library was built without compression support\n");
	fprintf(stderr, "Rebuild with at least one of: -DENABLE_ZLIB=ON, -DENABLE_BZIP2=ON, -DENABLE_LZMA=ON, -DENABLE_ZSTD=ON\n");
	return 1;
}
#else

/* Helper function to check if string ends with suffix */
static int ends_with(const char *str, const char *suffix)
{
	size_t str_len = strlen(str);
	size_t suffix_len = strlen(suffix);

	if (suffix_len > str_len)
		return 0;

	return strcmp(str + str_len - suffix_len, suffix) == 0;
}

/* Detect compression type from file extension */
static enum compression_type detect_compression_type(const char *filename)
{
	if (ends_with(filename, ".gz"))
		return COMPRESS_GZIP;
	else if (ends_with(filename, ".bz2"))
		return COMPRESS_BZIP2;
	else if (ends_with(filename, ".xz"))
		return COMPRESS_XZ;
	else if (ends_with(filename, ".zst"))
		return COMPRESS_ZSTD;
	else
		return COMPRESS_NONE;
}

/* Get file size */
static off64_t get_file_size(const char *filename)
{
	struct stat st;
	if (stat(filename, &st) < 0)
		return -1;
	return st.st_size;
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <input_file> <output_file.{gz|bz2|xz|zst}>\n", argv[0]);
		fprintf(stderr, "\nCompress a file using the format specified by the output extension.\n");
		fprintf(stderr, "\nSupported formats:\n");
		fprintf(stderr, "  .gz   - gzip compression (zlib)\n");
		fprintf(stderr, "  .bz2  - bzip2 compression\n");
		fprintf(stderr, "  .xz   - xz compression (LZMA)\n");
		fprintf(stderr, "  .zst  - zstd compression\n");
		return 1;
	}

	const char *input_file = argv[1];
	const char *output_file = argv[2];

	/* Print library info */
	printf("StreamIO version: %s\n", stream_get_version());
	printf("Features: %s\n\n", stream_get_features_string());

	/* Detect compression type */
	enum compression_type type = detect_compression_type(output_file);
	if (type == COMPRESS_NONE) {
		fprintf(stderr, "Error: Could not detect compression type from extension\n");
		fprintf(stderr, "Output file must end with: .gz, .bz2, .xz, or .zst\n");
		return 1;
	}

	/* Check if compression type is available */
	if (!compression_is_available(type)) {
		const char *format_name = "unknown";
		if (type == COMPRESS_GZIP) format_name = "gzip";
		else if (type == COMPRESS_BZIP2) format_name = "bzip2";
		else if (type == COMPRESS_XZ) format_name = "xz/lzma";
		else if (type == COMPRESS_ZSTD) format_name = "zstd";

		fprintf(stderr, "Error: %s compression not available\n", format_name);
		fprintf(stderr, "Library was built without this compression format.\n");
		return 1;
	}

	/* Get input file size before compression */
	off64_t input_size = get_file_size(input_file);
	if (input_size < 0) {
		fprintf(stderr, "Error: Could not stat input file '%s'\n", input_file);
		return 1;
	}

	printf("Compressing: %s -> %s\n", input_file, output_file);
	printf("Input size: %lld bytes\n", (long long)input_size);

	/* Open input file */
	struct file_stream input_fs;
	int ret = file_stream_open(&input_fs, input_file, O_RDONLY, 0);
	if (ret < 0) {
		fprintf(stderr, "Error: Failed to open input file '%s': %s\n",
			input_file, strerror(-ret));
		return 1;
	}

	/* Open output file */
	struct file_stream output_fs;
	ret = file_stream_open(&output_fs, output_file,
			       O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (ret < 0) {
		fprintf(stderr, "Error: Failed to open output file '%s': %s\n",
			output_file, strerror(-ret));
		stream_close(&input_fs.base);
		return 1;
	}

	/* Create compression stream */
	struct compression_stream cs;
	ret = compression_stream_init(&cs, &output_fs.base, type, O_WRONLY, 1);
	if (ret < 0) {
		fprintf(stderr, "Error: Failed to initialize compression stream: %s\n",
			strerror(-ret));
		stream_close(&output_fs.base);
		stream_close(&input_fs.base);
		return 1;
	}

	/* Compress data by reading from input and writing to compression stream */
	char buffer[65536];  /* 64KB buffer */
	ssize_t total_read = 0;
	ssize_t nread;

	printf("Compressing");
	fflush(stdout);

	while ((nread = stream_read(&input_fs.base, buffer, sizeof(buffer))) > 0) {
		ssize_t written = stream_write(&cs.base, buffer, nread);
		if (written < 0) {
			fprintf(stderr, "\nError: Failed to write compressed data: %s\n",
				strerror(-written));
			stream_close(&cs.base);
			stream_close(&input_fs.base);
			return 1;
		}
		if (written != nread) {
			fprintf(stderr, "\nError: Incomplete write (wrote %zd of %zd bytes)\n",
				written, nread);
			stream_close(&cs.base);
			stream_close(&input_fs.base);
			return 1;
		}

		total_read += nread;

		/* Progress indicator */
		if (total_read % (1024 * 1024) == 0 || total_read == input_size) {
			printf(".");
			fflush(stdout);
		}
	}

	if (nread < 0) {
		fprintf(stderr, "\nError: Failed to read input file: %s\n",
			strerror(-nread));
		stream_close(&cs.base);
		stream_close(&input_fs.base);
		return 1;
	}

	/* Close streams (this finalizes compression and flushes remaining data) */
	stream_close(&cs.base);   /* Also closes output_fs */
	stream_close(&input_fs.base);

	printf(" done!\n");

	/* Get output file size and calculate compression ratio */
	off64_t output_size = get_file_size(output_file);
	if (output_size < 0) {
		fprintf(stderr, "Warning: Could not stat output file\n");
	} else {
		printf("\nResults:\n");
		printf("  Input size:  %lld bytes\n", (long long)input_size);
		printf("  Output size: %lld bytes\n", (long long)output_size);
		if (input_size > 0) {
			double ratio = (double)output_size / (double)input_size * 100.0;
			printf("  Compression: %.1f%%\n", ratio);
			printf("  Saved:       %lld bytes (%.1f%%)\n",
			       (long long)(input_size - output_size),
			       100.0 - ratio);
		}
	}

	return 0;
}
#endif
