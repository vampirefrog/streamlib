/**
 * @file create_archive.c
 * @brief Example of creating archives in different formats
 */

#include <stream.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#ifndef HAVE_LIBARCHIVE
int main(void)
{
	fprintf(stderr, "Error: This library was built without libarchive support\n");
	fprintf(stderr, "Rebuild with -DENABLE_LIBARCHIVE=ON\n");
	return 1;
}
#else

/* Detect archive format from file extension */
static enum streamio_archive_format detect_format(const char *filename)
{
	const char *ext = strrchr(filename, '.');
	if (!ext)
		return STREAMIO_ARCHIVE_TAR_PAX;  /* Default to TAR */

	if (strcmp(ext, ".tar") == 0)
		return STREAMIO_ARCHIVE_TAR_PAX;
	else if (strcmp(ext, ".zip") == 0)
		return STREAMIO_ARCHIVE_ZIP;
	else if (strcmp(ext, ".7z") == 0)
		return STREAMIO_ARCHIVE_7ZIP;
	else if (strcmp(ext, ".cpio") == 0)
		return STREAMIO_ARCHIVE_CPIO;
	else if (strcmp(ext, ".iso") == 0)
		return STREAMIO_ARCHIVE_ISO9660;
	else
		return STREAMIO_ARCHIVE_TAR_PAX;  /* Default */
}

/* Get file size */
static off64_t get_file_size(const char *filename)
{
	struct stat st;
	if (stat(filename, &st) < 0)
		return -1;
	return st.st_size;
}

/* Add a file to the archive */
static int add_file_to_archive(struct archive_stream *ar, const char *path)
{
	printf("  Adding: %s ... ", path);
	fflush(stdout);

	/* Get file info */
	struct stat st;
	if (stat(path, &st) < 0) {
		printf("FAILED (stat error: %s)\n", strerror(errno));
		return -1;
	}

	/* Skip directories for now */
	if (S_ISDIR(st.st_mode)) {
		printf("SKIPPED (directory)\n");
		return 0;
	}

	/* Create entry in archive */
	int ret = archive_stream_new_entry(ar, path, st.st_mode, st.st_size);
	if (ret < 0) {
		printf("FAILED (new_entry error: %s)\n", strerror(-ret));
		return ret;
	}

	/* Open source file */
	struct file_stream fs;
	ret = file_stream_open(&fs, path, O_RDONLY, 0);
	if (ret < 0) {
		printf("FAILED (open error: %s)\n", strerror(-ret));
		archive_stream_finish_entry(ar);
		return ret;
	}

	/* Copy file content to archive */
	char buffer[65536];
	ssize_t total_written = 0;
	ssize_t nread;

	while ((nread = stream_read(&fs.base, buffer, sizeof(buffer))) > 0) {
		ssize_t written = archive_stream_write_data(ar, buffer, nread);
		if (written < 0) {
			printf("FAILED (write error)\n");
			stream_close(&fs.base);
			archive_stream_finish_entry(ar);
			return written;
		}
		total_written += written;
	}

	stream_close(&fs.base);

	/* Finish entry */
	ret = archive_stream_finish_entry(ar);
	if (ret < 0) {
		printf("FAILED (finish_entry error: %s)\n", strerror(-ret));
		return ret;
	}

	printf("OK (%lld bytes)\n", (long long)total_written);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "Usage: %s <output-archive> <file1> [file2 ...]\n", argv[0]);
		fprintf(stderr, "\n");
		fprintf(stderr, "Create an archive from the specified files.\n");
		fprintf(stderr, "Output format is determined by file extension:\n");
		fprintf(stderr, "  .tar  - TAR (POSIX.1-2001 format)\n");
		fprintf(stderr, "  .zip  - ZIP\n");
		fprintf(stderr, "  .7z   - 7-Zip\n");
		fprintf(stderr, "  .cpio - CPIO\n");
		fprintf(stderr, "  .iso  - ISO9660\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "Examples:\n");
		fprintf(stderr, "  %s myfiles.tar file1.txt file2.txt\n", argv[0]);
		fprintf(stderr, "  %s backup.zip *.c *.h\n", argv[0]);
		return 1;
	}

	const char *output_file = argv[1];

	/* Print library info */
	printf("StreamIO Archive Creator\n");
	printf("========================\n\n");
	printf("Version: %s\n", stream_get_version());
	printf("Features: %s\n\n", stream_get_features_string());

	/* Detect format */
	enum streamio_archive_format format = detect_format(output_file);
	const char *format_name;
	switch (format) {
	case STREAMIO_ARCHIVE_TAR_USTAR:
		format_name = "TAR (USTAR)";
		break;
	case STREAMIO_ARCHIVE_TAR_PAX:
		format_name = "TAR (PAX)";
		break;
	case STREAMIO_ARCHIVE_ZIP:
		format_name = "ZIP";
		break;
	case STREAMIO_ARCHIVE_7ZIP:
		format_name = "7-Zip";
		break;
	case STREAMIO_ARCHIVE_CPIO:
		format_name = "CPIO";
		break;
	case STREAMIO_ARCHIVE_SHAR:
		format_name = "SHAR";
		break;
	case STREAMIO_ARCHIVE_ISO9660:
		format_name = "ISO9660";
		break;
	default:
		format_name = "Unknown";
	}

	printf("Creating %s archive: %s\n", format_name, output_file);
	printf("Files to add: %d\n\n", argc - 2);

	/* Check if format is available */
	if (!archive_format_available(format)) {
		fprintf(stderr, "Error: %s format not available\n", format_name);
		return 1;
	}

	/* Open output file */
	struct file_stream fs;
	int ret = file_stream_open(&fs, output_file,
				    O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (ret < 0) {
		fprintf(stderr, "Error: Failed to open output file '%s': %s\n",
			output_file, strerror(-ret));
		return 1;
	}

	/* Create archive stream */
	struct archive_stream ar;
	ret = archive_stream_open_write(&ar, &fs.base, format, 1);
	if (ret < 0) {
		fprintf(stderr, "Error: Failed to create archive: %s\n",
			strerror(-ret));
		stream_close(&fs.base);
		return 1;
	}

	/* Add each file */
	int files_added = 0;
	int files_failed = 0;

	for (int i = 2; i < argc; i++) {
		ret = add_file_to_archive(&ar, argv[i]);
		if (ret < 0) {
			files_failed++;
		} else if (ret == 0) {
			files_added++;
		}
	}

	/* Close and finalize archive */
	printf("\nFinalizing archive...\n");
	archive_stream_close(&ar);

	/* Get final archive size */
	off64_t archive_size = get_file_size(output_file);

	printf("\nArchive created successfully!\n");
	printf("  Output: %s\n", output_file);
	printf("  Format: %s\n", format_name);
	printf("  Files added: %d\n", files_added);
	if (files_failed > 0)
		printf("  Files failed: %d\n", files_failed);
	if (archive_size >= 0)
		printf("  Size: %lld bytes\n", (long long)archive_size);

	return files_failed > 0 ? 1 : 0;
}

#endif /* HAVE_LIBARCHIVE */
