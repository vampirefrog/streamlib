/**
 * @file list_archive.c
 * @brief Example showing how to list contents of archive files
 */

#include <stream.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef STREAMIO_HAVE_LIBARCHIVE

/* Callback to print each entry */
static int print_entry(const struct archive_entry_info *entry, void *userdata)
{
	int *count = userdata;
	(*count)++;

	/* Print entry details */
	printf("  %c  %10lld  %s",
	       entry->is_dir ? 'd' : '-',
	       (long long)entry->size,
	       entry->pathname);

	if (entry->is_compressed)
		printf(" (compressed)");

	printf("\n");

	return 0;
}

#endif /* STREAMIO_HAVE_LIBARCHIVE */

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <archive>\n", argv[0]);
		fprintf(stderr, "Supported formats: tar, tar.gz, tar.bz2, zip, etc.\n");
		return 1;
	}

	const char *filename = argv[1];

	/* Print library info */
	printf("StreamIO version: %s\n", stream_get_version());
	printf("Features: %s\n\n", stream_get_features_string());

#ifndef STREAMIO_HAVE_LIBARCHIVE
	fprintf(stderr, "Error: This library was built without libarchive support\n");
	fprintf(stderr, "Rebuild with -DSTREAMIO_ENABLE_LIBARCHIVE=ON\n");
	return 1;
#else

	int ret;
	struct file_stream fs;
	struct archive_stream ar;
	int entry_count = 0;

	/* Check if this is a compressed archive */
	int is_gzip = (strstr(filename, ".gz") != NULL);
	int is_bzip2 = (strstr(filename, ".bz2") != NULL);

	/* Open file */
	ret = file_stream_open(&fs, filename, O_RDONLY, 0);
	if (ret < 0) {
		fprintf(stderr, "Failed to open file '%s': %s\n",
			filename, strerror(-ret));
		return 1;
	}

	/* Handle compressed archives if we have compression support */
#ifdef STREAMIO_HAVE_ZLIB
	struct compression_stream cs;
	int using_compression = 0;

	if (is_gzip && compression_is_available(COMPRESS_GZIP)) {
		printf("Detected gzip compression, decompressing...\n");
		ret = gzip_stream_init(&cs, &fs.base, O_RDONLY, 1);
		if (ret < 0) {
			fprintf(stderr, "Failed to init gzip stream: %s\n",
				strerror(-ret));
			stream_close(&fs.base);
			return 1;
		}
		using_compression = 1;

		/* Open archive from decompressed stream */
		ret = archive_stream_open(&ar, &cs.base, 1);
	} else
#endif
	{
		if (is_gzip || is_bzip2) {
			fprintf(stderr, "Warning: File appears compressed but compression support not available\n");
		}

		/* Open archive directly from file */
		ret = archive_stream_open(&ar, &fs.base, 1);
	}

	if (ret < 0) {
		fprintf(stderr, "Failed to open archive: %s\n", strerror(-ret));
#ifdef STREAMIO_HAVE_ZLIB
		if (using_compression)
			stream_close(&cs.base);
		else
#endif
			stream_close(&fs.base);
		return 1;
	}

	printf("Archive: %s\n", filename);
	printf("-------------------------------------------\n");
	printf("Type      Size      Name\n");
	printf("-------------------------------------------\n");

	/* Walk through archive entries */
	ret = archive_stream_walk(&ar, print_entry, &entry_count);
	if (ret < 0) {
		fprintf(stderr, "Error walking archive: %s\n", strerror(-ret));
		archive_stream_close(&ar);
		return 1;
	}

	printf("-------------------------------------------\n");
	printf("Total entries: %d\n", entry_count);

	/* Close archive */
	archive_stream_close(&ar);

	return 0;
#endif
}
