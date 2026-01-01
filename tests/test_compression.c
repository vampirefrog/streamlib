/**
 * @file test_compression.c
 * @brief Tests for compression support
 */

#include <stream.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#define TEST_FILE "/tmp/streamio_compress_test.txt"
#define TEST_FILE_GZ "/tmp/streamio_compress_test.txt.gz"
#define TEST_FILE_BZ2 "/tmp/streamio_compress_test.txt.bz2"
#define TEST_FILE_XZ "/tmp/streamio_compress_test.txt.xz"
#define TEST_FILE_ZST "/tmp/streamio_compress_test.txt.zst"

static int test_count = 0;
static int test_passed = 0;

#define TEST(name) \
	do { \
		printf("Running test: %s ... ", name); \
		test_count++; \
	} while (0)

#define PASS() \
	do { \
		printf("PASS\n"); \
		test_passed++; \
	} while (0)

#define FAIL(msg) \
	do { \
		printf("FAIL: %s\n", msg); \
		return; \
	} while (0)

#ifdef STREAMIO_HAVE_ZLIB

/* Test compression availability detection */
void test_compression_available(void)
{
	TEST("compression_available");

	if (!compression_is_available(COMPRESS_GZIP))
		FAIL("GZIP should be available");

	if (!stream_has_feature(STREAMIO_FEAT_ZLIB))
		FAIL("ZLIB feature should be available");

	PASS();
}

/* Test writing compressed data */
void test_gzip_write(void)
{
	TEST("gzip_write");

	/* Use larger, more compressible data */
	const char *test_data =
		"This is test data that will be compressed using gzip! "
		"Repeating text compresses very well. "
		"This is test data that will be compressed using gzip! "
		"Repeating text compresses very well. "
		"This is test data that will be compressed using gzip! "
		"Repeating text compresses very well. "
		"This is test data that will be compressed using gzip! "
		"Repeating text compresses very well.";

	/* Open output file */
	struct file_stream fs;
	int ret = file_stream_open(&fs, TEST_FILE_GZ,
				   O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (ret < 0)
		FAIL("Failed to open output file");

	/* Create gzip compression stream */
	struct compression_stream cs;
	ret = gzip_stream_init(&cs, &fs.base, O_WRONLY, 1);
	if (ret < 0) {
		stream_close(&fs.base);
		FAIL("Failed to init gzip stream");
	}

	/* Write compressed data */
	ssize_t written = stream_write(&cs.base, test_data, strlen(test_data));
	if (written != (ssize_t)strlen(test_data)) {
		stream_close(&cs.base);
		FAIL("Write failed");
	}

	/* Close (this flushes the compression) */
	stream_close(&cs.base);

	/* Verify the file exists and has some data */
	struct file_stream check;
	ret = file_stream_open(&check, TEST_FILE_GZ, O_RDONLY, 0);
	if (ret < 0)
		FAIL("Compressed file doesn't exist");

	off64_t size = stream_size(&check.base);
	stream_close(&check.base);

	if (size <= 0)
		FAIL("Compressed file is empty");

	/* Compressed size should be smaller than original for repetitive data */
	if ((size_t)size >= strlen(test_data))
		FAIL("Compressed size not smaller than original");

	PASS();
}

/* Test reading compressed data */
void test_gzip_read(void)
{
	TEST("gzip_read");

	const char *test_data = "This is test data that will be compressed using gzip!";

	/* First, ensure we have a compressed file */
	struct file_stream fs;
	int ret = file_stream_open(&fs, TEST_FILE_GZ,
				   O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (ret < 0)
		FAIL("Failed to create test file");

	struct compression_stream cs;
	ret = gzip_stream_init(&cs, &fs.base, O_WRONLY, 1);
	if (ret < 0) {
		stream_close(&fs.base);
		FAIL("Failed to init gzip stream for writing");
	}

	stream_write(&cs.base, test_data, strlen(test_data));
	stream_close(&cs.base);

	/* Now read it back */
	ret = file_stream_open(&fs, TEST_FILE_GZ, O_RDONLY, 0);
	if (ret < 0)
		FAIL("Failed to open compressed file");

	ret = gzip_stream_init(&cs, &fs.base, O_RDONLY, 1);
	if (ret < 0) {
		stream_close(&fs.base);
		FAIL("Failed to init gzip stream for reading");
	}

	/* Read decompressed data */
	char buf[1024];
	ssize_t nread = stream_read(&cs.base, buf, sizeof(buf));
	if (nread < 0) {
		stream_close(&cs.base);
		FAIL("Read failed");
	}

	buf[nread] = '\0';

	if (strcmp(buf, test_data) != 0) {
		stream_close(&cs.base);
		FAIL("Decompressed data doesn't match");
	}

	/* Verify we're at EOF */
	nread = stream_read(&cs.base, buf, sizeof(buf));
	if (nread != 0) {
		stream_close(&cs.base);
		FAIL("Should be at EOF");
	}

	stream_close(&cs.base);
	unlink(TEST_FILE_GZ);

	PASS();
}

/* Test auto-detection of gzip format */
void test_gzip_auto_detect(void)
{
	TEST("gzip_auto_detect");

	const char *test_data = "Auto-detection test data";

	/* Create a gzipped file */
	struct file_stream fs;
	int ret = file_stream_open(&fs, TEST_FILE_GZ,
				   O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (ret < 0)
		FAIL("Failed to create test file");

	struct compression_stream cs;
	ret = gzip_stream_init(&cs, &fs.base, O_WRONLY, 1);
	if (ret < 0) {
		stream_close(&fs.base);
		FAIL("Failed to init gzip stream");
	}

	stream_write(&cs.base, test_data, strlen(test_data));
	stream_close(&cs.base);

	/* Now try to auto-detect and read */
	ret = file_stream_open(&fs, TEST_FILE_GZ, O_RDONLY, 0);
	if (ret < 0)
		FAIL("Failed to open file for reading");

	ret = compression_stream_auto(&cs, &fs.base, 1);
	if (ret < 0) {
		stream_close(&fs.base);
		FAIL("Auto-detection failed");
	}

	/* Read and verify */
	char buf[1024];
	ssize_t nread = stream_read(&cs.base, buf, sizeof(buf));
	if (nread < 0) {
		stream_close(&cs.base);
		FAIL("Read failed");
	}

	buf[nread] = '\0';

	if (strcmp(buf, test_data) != 0) {
		stream_close(&cs.base);
		FAIL("Data mismatch");
	}

	stream_close(&cs.base);
	unlink(TEST_FILE_GZ);

	PASS();
}

/* Test round-trip compression/decompression */
void test_gzip_roundtrip(void)
{
	TEST("gzip_roundtrip");

	const char *test_data = "The quick brown fox jumps over the lazy dog. "
				"This sentence repeats. "
				"The quick brown fox jumps over the lazy dog. "
				"This sentence repeats. "
				"The quick brown fox jumps over the lazy dog.";

	/* Compress to memory stream */
	struct mem_stream ms_out;
	mem_stream_create(&ms_out, 0);

	struct compression_stream cs_write;
	int ret = gzip_stream_init(&cs_write, &ms_out.base, O_WRONLY, 0);
	if (ret < 0) {
		stream_close(&ms_out.base);
		FAIL("Failed to init compression");
	}

	ssize_t written = stream_write(&cs_write.base, test_data, strlen(test_data));
	if (written != (ssize_t)strlen(test_data)) {
		stream_close(&cs_write.base);
		stream_close(&ms_out.base);
		FAIL("Write failed");
	}

	stream_close(&cs_write.base);

	/* Get compressed data */
	size_t compressed_size;
	const void *compressed_data = mem_stream_get_buffer(&ms_out, &compressed_size);

	/* Decompress from memory stream */
	struct mem_stream ms_in;
	mem_stream_init(&ms_in, (void *)compressed_data, compressed_size, 0);

	struct compression_stream cs_read;
	ret = gzip_stream_init(&cs_read, &ms_in.base, O_RDONLY, 0);
	if (ret < 0) {
		stream_close(&ms_out.base);
		stream_close(&ms_in.base);
		FAIL("Failed to init decompression");
	}

	/* Read back */
	char buf[1024];
	ssize_t nread = stream_read(&cs_read.base, buf, sizeof(buf));
	if (nread < 0) {
		stream_close(&cs_read.base);
		stream_close(&ms_out.base);
		stream_close(&ms_in.base);
		FAIL("Read failed");
	}

	buf[nread] = '\0';

	if (strcmp(buf, test_data) != 0) {
		stream_close(&cs_read.base);
		stream_close(&ms_out.base);
		stream_close(&ms_in.base);
		FAIL("Data mismatch");
	}

	stream_close(&cs_read.base);
	stream_close(&ms_out.base);
	stream_close(&ms_in.base);

	PASS();
}

#endif /* STREAMIO_HAVE_ZLIB */

#ifdef STREAMIO_HAVE_BZIP2

/* Test bzip2 availability */
void test_bzip2_available(void)
{
	TEST("bzip2_available");

	if (!compression_is_available(COMPRESS_BZIP2))
		FAIL("BZIP2 should be available");

	if (!stream_has_feature(STREAMIO_FEAT_BZIP2))
		FAIL("BZIP2 feature should be available");

	PASS();
}

/* Test writing bzip2 compressed data */
void test_bzip2_write(void)
{
	TEST("bzip2_write");

	const char *test_data =
		"This is test data that will be compressed using bzip2! "
		"Repeating text compresses very well. "
		"This is test data that will be compressed using bzip2! "
		"Repeating text compresses very well. "
		"This is test data that will be compressed using bzip2! "
		"Repeating text compresses very well. "
		"This is test data that will be compressed using bzip2! "
		"Repeating text compresses very well.";

	/* Open output file */
	struct file_stream fs;
	int ret = file_stream_open(&fs, TEST_FILE_BZ2,
				   O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (ret < 0)
		FAIL("Failed to open output file");

	/* Create bzip2 compression stream */
	struct compression_stream cs;
	ret = compression_stream_init(&cs, &fs.base, COMPRESS_BZIP2, O_WRONLY, 1);
	if (ret < 0) {
		stream_close(&fs.base);
		FAIL("Failed to init bzip2 stream");
	}

	/* Write compressed data */
	ssize_t written = stream_write(&cs.base, test_data, strlen(test_data));
	if (written != (ssize_t)strlen(test_data)) {
		stream_close(&cs.base);
		FAIL("Write failed");
	}

	/* Close (this flushes the compression) */
	stream_close(&cs.base);

	/* Verify the file exists and has some data */
	struct file_stream check;
	ret = file_stream_open(&check, TEST_FILE_BZ2, O_RDONLY, 0);
	if (ret < 0)
		FAIL("Compressed file doesn't exist");

	off64_t size = stream_size(&check.base);
	stream_close(&check.base);

	if (size <= 0)
		FAIL("Compressed file is empty");

	/* Compressed size should be smaller than original for repetitive data */
	if ((size_t)size >= strlen(test_data))
		FAIL("Compressed size not smaller than original");

	PASS();
}

/* Test reading bzip2 compressed data */
void test_bzip2_read(void)
{
	TEST("bzip2_read");

	const char *test_data = "This is test data that will be compressed using bzip2!";

	/* First, ensure we have a compressed file */
	struct file_stream fs;
	int ret = file_stream_open(&fs, TEST_FILE_BZ2,
				   O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (ret < 0)
		FAIL("Failed to create test file");

	struct compression_stream cs;
	ret = compression_stream_init(&cs, &fs.base, COMPRESS_BZIP2, O_WRONLY, 1);
	if (ret < 0) {
		stream_close(&fs.base);
		FAIL("Failed to init bzip2 stream for writing");
	}

	stream_write(&cs.base, test_data, strlen(test_data));
	stream_close(&cs.base);

	/* Now read it back */
	ret = file_stream_open(&fs, TEST_FILE_BZ2, O_RDONLY, 0);
	if (ret < 0)
		FAIL("Failed to open compressed file");

	ret = compression_stream_init(&cs, &fs.base, COMPRESS_BZIP2, O_RDONLY, 1);
	if (ret < 0) {
		stream_close(&fs.base);
		FAIL("Failed to init bzip2 stream for reading");
	}

	/* Read decompressed data */
	char buf[1024];
	ssize_t nread = stream_read(&cs.base, buf, sizeof(buf));
	if (nread < 0) {
		stream_close(&cs.base);
		FAIL("Read failed");
	}

	buf[nread] = '\0';

	if (strcmp(buf, test_data) != 0) {
		stream_close(&cs.base);
		FAIL("Decompressed data doesn't match");
	}

	/* Verify we're at EOF */
	nread = stream_read(&cs.base, buf, sizeof(buf));
	if (nread != 0) {
		stream_close(&cs.base);
		FAIL("Should be at EOF");
	}

	stream_close(&cs.base);
	unlink(TEST_FILE_BZ2);

	PASS();
}

/* Test auto-detection of bzip2 format */
void test_bzip2_auto_detect(void)
{
	TEST("bzip2_auto_detect");

	const char *test_data = "Auto-detection test data for bzip2";

	/* Create a bzip2 file */
	struct file_stream fs;
	int ret = file_stream_open(&fs, TEST_FILE_BZ2,
				   O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (ret < 0)
		FAIL("Failed to create test file");

	struct compression_stream cs;
	ret = compression_stream_init(&cs, &fs.base, COMPRESS_BZIP2, O_WRONLY, 1);
	if (ret < 0) {
		stream_close(&fs.base);
		FAIL("Failed to init bzip2 stream");
	}

	stream_write(&cs.base, test_data, strlen(test_data));
	stream_close(&cs.base);

	/* Now try to auto-detect and read */
	ret = file_stream_open(&fs, TEST_FILE_BZ2, O_RDONLY, 0);
	if (ret < 0)
		FAIL("Failed to open file for reading");

	ret = compression_stream_auto(&cs, &fs.base, 1);
	if (ret < 0) {
		stream_close(&fs.base);
		FAIL("Auto-detection failed");
	}

	/* Read and verify */
	char buf[1024];
	ssize_t nread = stream_read(&cs.base, buf, sizeof(buf));
	if (nread < 0) {
		stream_close(&cs.base);
		FAIL("Read failed");
	}

	buf[nread] = '\0';

	if (strcmp(buf, test_data) != 0) {
		stream_close(&cs.base);
		FAIL("Data mismatch");
	}

	stream_close(&cs.base);
	unlink(TEST_FILE_BZ2);

	PASS();
}

/* Test bzip2 round-trip compression/decompression */
void test_bzip2_roundtrip(void)
{
	TEST("bzip2_roundtrip");

	const char *test_data = "The quick brown fox jumps over the lazy dog. "
				"This sentence repeats with bzip2. "
				"The quick brown fox jumps over the lazy dog. "
				"This sentence repeats with bzip2. "
				"The quick brown fox jumps over the lazy dog.";

	/* Compress to memory stream */
	struct mem_stream ms_out;
	mem_stream_create(&ms_out, 0);

	struct compression_stream cs_write;
	int ret = compression_stream_init(&cs_write, &ms_out.base, COMPRESS_BZIP2,
					  O_WRONLY, 0);
	if (ret < 0) {
		stream_close(&ms_out.base);
		FAIL("Failed to init compression");
	}

	ssize_t written = stream_write(&cs_write.base, test_data, strlen(test_data));
	if (written != (ssize_t)strlen(test_data)) {
		stream_close(&cs_write.base);
		stream_close(&ms_out.base);
		FAIL("Write failed");
	}

	stream_close(&cs_write.base);

	/* Get compressed data */
	size_t compressed_size;
	const void *compressed_data = mem_stream_get_buffer(&ms_out, &compressed_size);

	/* Decompress from memory stream */
	struct mem_stream ms_in;
	mem_stream_init(&ms_in, (void *)compressed_data, compressed_size, 0);

	struct compression_stream cs_read;
	ret = compression_stream_init(&cs_read, &ms_in.base, COMPRESS_BZIP2,
				      O_RDONLY, 0);
	if (ret < 0) {
		stream_close(&ms_out.base);
		stream_close(&ms_in.base);
		FAIL("Failed to init decompression");
	}

	/* Read back */
	char buf[1024];
	ssize_t nread = stream_read(&cs_read.base, buf, sizeof(buf));
	if (nread < 0) {
		stream_close(&cs_read.base);
		stream_close(&ms_out.base);
		stream_close(&ms_in.base);
		FAIL("Read failed");
	}

	buf[nread] = '\0';

	if (strcmp(buf, test_data) != 0) {
		stream_close(&cs_read.base);
		stream_close(&ms_out.base);
		stream_close(&ms_in.base);
		FAIL("Data mismatch");
	}

	stream_close(&cs_read.base);
	stream_close(&ms_out.base);
	stream_close(&ms_in.base);

	PASS();
}

#endif /* STREAMIO_HAVE_BZIP2 */

#ifdef STREAMIO_HAVE_LZMA

/* Test xz/lzma availability */
void test_xz_available(void)
{
	TEST("xz_available");

	if (!compression_is_available(COMPRESS_XZ))
		FAIL("XZ should be available");

	if (!stream_has_feature(STREAMIO_FEAT_LZMA))
		FAIL("LZMA feature should be available");

	PASS();
}

/* Test writing xz compressed data */
void test_xz_write(void)
{
	TEST("xz_write");

	const char *test_data =
		"This is test data that will be compressed using xz! "
		"Repeating text compresses very well. "
		"This is test data that will be compressed using xz! "
		"Repeating text compresses very well. "
		"This is test data that will be compressed using xz! "
		"Repeating text compresses very well. "
		"This is test data that will be compressed using xz! "
		"Repeating text compresses very well.";

	/* Open output file */
	struct file_stream fs;
	int ret = file_stream_open(&fs, TEST_FILE_XZ,
				   O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (ret < 0)
		FAIL("Failed to open output file");

	/* Create xz compression stream */
	struct compression_stream cs;
	ret = compression_stream_init(&cs, &fs.base, COMPRESS_XZ, O_WRONLY, 1);
	if (ret < 0) {
		stream_close(&fs.base);
		FAIL("Failed to init xz stream");
	}

	/* Write compressed data */
	ssize_t written = stream_write(&cs.base, test_data, strlen(test_data));
	if (written != (ssize_t)strlen(test_data)) {
		stream_close(&cs.base);
		FAIL("Write failed");
	}

	/* Close (this flushes the compression) */
	stream_close(&cs.base);

	/* Verify the file exists and has some data */
	struct file_stream check;
	ret = file_stream_open(&check, TEST_FILE_XZ, O_RDONLY, 0);
	if (ret < 0)
		FAIL("Compressed file doesn't exist");

	off64_t size = stream_size(&check.base);
	stream_close(&check.base);

	if (size <= 0)
		FAIL("Compressed file is empty");

	/* Compressed size should be smaller than original for repetitive data */
	if ((size_t)size >= strlen(test_data))
		FAIL("Compressed size not smaller than original");

	PASS();
}

/* Test reading xz compressed data */
void test_xz_read(void)
{
	TEST("xz_read");

	const char *test_data = "This is test data that will be compressed using xz!";

	/* First, ensure we have a compressed file */
	struct file_stream fs;
	int ret = file_stream_open(&fs, TEST_FILE_XZ,
				   O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (ret < 0)
		FAIL("Failed to create test file");

	struct compression_stream cs;
	ret = compression_stream_init(&cs, &fs.base, COMPRESS_XZ, O_WRONLY, 1);
	if (ret < 0) {
		stream_close(&fs.base);
		FAIL("Failed to init xz stream for writing");
	}

	stream_write(&cs.base, test_data, strlen(test_data));
	stream_close(&cs.base);

	/* Now read it back */
	ret = file_stream_open(&fs, TEST_FILE_XZ, O_RDONLY, 0);
	if (ret < 0)
		FAIL("Failed to open compressed file");

	ret = compression_stream_init(&cs, &fs.base, COMPRESS_XZ, O_RDONLY, 1);
	if (ret < 0) {
		stream_close(&fs.base);
		FAIL("Failed to init xz stream for reading");
	}

	/* Read decompressed data */
	char buf[1024];
	ssize_t nread = stream_read(&cs.base, buf, sizeof(buf));
	if (nread < 0) {
		stream_close(&cs.base);
		FAIL("Read failed");
	}

	buf[nread] = '\0';

	if (strcmp(buf, test_data) != 0) {
		stream_close(&cs.base);
		FAIL("Decompressed data doesn't match");
	}

	/* Verify we're at EOF */
	nread = stream_read(&cs.base, buf, sizeof(buf));
	if (nread != 0) {
		stream_close(&cs.base);
		FAIL("Should be at EOF");
	}

	stream_close(&cs.base);
	unlink(TEST_FILE_XZ);

	PASS();
}

/* Test auto-detection of xz format */
void test_xz_auto_detect(void)
{
	TEST("xz_auto_detect");

	const char *test_data = "Auto-detection test data for xz";

	/* Create an xz file */
	struct file_stream fs;
	int ret = file_stream_open(&fs, TEST_FILE_XZ,
				   O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (ret < 0)
		FAIL("Failed to create test file");

	struct compression_stream cs;
	ret = compression_stream_init(&cs, &fs.base, COMPRESS_XZ, O_WRONLY, 1);
	if (ret < 0) {
		stream_close(&fs.base);
		FAIL("Failed to init xz stream");
	}

	stream_write(&cs.base, test_data, strlen(test_data));
	stream_close(&cs.base);

	/* Now try to auto-detect and read */
	ret = file_stream_open(&fs, TEST_FILE_XZ, O_RDONLY, 0);
	if (ret < 0)
		FAIL("Failed to open file for reading");

	ret = compression_stream_auto(&cs, &fs.base, 1);
	if (ret < 0) {
		stream_close(&fs.base);
		FAIL("Auto-detection failed");
	}

	/* Read and verify */
	char buf[1024];
	ssize_t nread = stream_read(&cs.base, buf, sizeof(buf));
	if (nread < 0) {
		stream_close(&cs.base);
		FAIL("Read failed");
	}

	buf[nread] = '\0';

	if (strcmp(buf, test_data) != 0) {
		stream_close(&cs.base);
		FAIL("Data mismatch");
	}

	stream_close(&cs.base);
	unlink(TEST_FILE_XZ);

	PASS();
}

/* Test xz round-trip compression/decompression */
void test_xz_roundtrip(void)
{
	TEST("xz_roundtrip");

	const char *test_data = "The quick brown fox jumps over the lazy dog. "
				"This sentence repeats with xz. "
				"The quick brown fox jumps over the lazy dog. "
				"This sentence repeats with xz. "
				"The quick brown fox jumps over the lazy dog.";

	/* Compress to memory stream */
	struct mem_stream ms_out;
	mem_stream_create(&ms_out, 0);

	struct compression_stream cs_write;
	int ret = compression_stream_init(&cs_write, &ms_out.base, COMPRESS_XZ,
					  O_WRONLY, 0);
	if (ret < 0) {
		stream_close(&ms_out.base);
		FAIL("Failed to init compression");
	}

	ssize_t written = stream_write(&cs_write.base, test_data, strlen(test_data));
	if (written != (ssize_t)strlen(test_data)) {
		stream_close(&cs_write.base);
		stream_close(&ms_out.base);
		FAIL("Write failed");
	}

	stream_close(&cs_write.base);

	/* Get compressed data */
	size_t compressed_size;
	const void *compressed_data = mem_stream_get_buffer(&ms_out, &compressed_size);

	/* Decompress from memory stream */
	struct mem_stream ms_in;
	mem_stream_init(&ms_in, (void *)compressed_data, compressed_size, 0);

	struct compression_stream cs_read;
	ret = compression_stream_init(&cs_read, &ms_in.base, COMPRESS_XZ,
				      O_RDONLY, 0);
	if (ret < 0) {
		stream_close(&ms_out.base);
		stream_close(&ms_in.base);
		FAIL("Failed to init decompression");
	}

	/* Read back */
	char buf[1024];
	ssize_t nread = stream_read(&cs_read.base, buf, sizeof(buf));
	if (nread < 0) {
		stream_close(&cs_read.base);
		stream_close(&ms_out.base);
		stream_close(&ms_in.base);
		FAIL("Read failed");
	}

	buf[nread] = '\0';

	if (strcmp(buf, test_data) != 0) {
		stream_close(&cs_read.base);
		stream_close(&ms_out.base);
		stream_close(&ms_in.base);
		FAIL("Data mismatch");
	}

	stream_close(&cs_read.base);
	stream_close(&ms_out.base);
	stream_close(&ms_in.base);

	PASS();
}

#endif /* STREAMIO_HAVE_LZMA */

#ifdef STREAMIO_HAVE_ZSTD

/* Test zstd availability */
void test_zstd_available(void)
{
	TEST("zstd_available");

	if (!compression_is_available(COMPRESS_ZSTD))
		FAIL("ZSTD should be available");

	if (!stream_has_feature(STREAMIO_FEAT_ZSTD))
		FAIL("ZSTD feature should be available");

	PASS();
}

/* Test writing zstd compressed data */
void test_zstd_write(void)
{
	TEST("zstd_write");

	const char *test_data =
		"This is test data that will be compressed using zstd! "
		"Repeating text compresses very well. "
		"This is test data that will be compressed using zstd! "
		"Repeating text compresses very well. "
		"This is test data that will be compressed using zstd! "
		"Repeating text compresses very well. "
		"This is test data that will be compressed using zstd! "
		"Repeating text compresses very well.";

	/* Open output file */
	struct file_stream fs;
	int ret = file_stream_open(&fs, TEST_FILE_ZST,
				   O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (ret < 0)
		FAIL("Failed to open output file");

	/* Create zstd compression stream */
	struct compression_stream cs;
	ret = compression_stream_init(&cs, &fs.base, COMPRESS_ZSTD, O_WRONLY, 1);
	if (ret < 0) {
		stream_close(&fs.base);
		FAIL("Failed to init zstd stream");
	}

	/* Write compressed data */
	ssize_t written = stream_write(&cs.base, test_data, strlen(test_data));
	if (written != (ssize_t)strlen(test_data)) {
		stream_close(&cs.base);
		FAIL("Write failed");
	}

	/* Close (this flushes the compression) */
	stream_close(&cs.base);

	/* Verify the file exists and has some data */
	struct file_stream check;
	ret = file_stream_open(&check, TEST_FILE_ZST, O_RDONLY, 0);
	if (ret < 0)
		FAIL("Compressed file doesn't exist");

	off64_t size = stream_size(&check.base);
	stream_close(&check.base);

	if (size <= 0)
		FAIL("Compressed file is empty");

	/* Compressed size should be smaller than original for repetitive data */
	if ((size_t)size >= strlen(test_data))
		FAIL("Compressed size not smaller than original");

	PASS();
}

/* Test reading zstd compressed data */
void test_zstd_read(void)
{
	TEST("zstd_read");

	const char *test_data = "This is test data that will be compressed using zstd!";

	/* First, ensure we have a compressed file */
	struct file_stream fs;
	int ret = file_stream_open(&fs, TEST_FILE_ZST,
				   O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (ret < 0)
		FAIL("Failed to create test file");

	struct compression_stream cs;
	ret = compression_stream_init(&cs, &fs.base, COMPRESS_ZSTD, O_WRONLY, 1);
	if (ret < 0) {
		stream_close(&fs.base);
		FAIL("Failed to init zstd stream for writing");
	}

	stream_write(&cs.base, test_data, strlen(test_data));
	stream_close(&cs.base);

	/* Now read it back */
	ret = file_stream_open(&fs, TEST_FILE_ZST, O_RDONLY, 0);
	if (ret < 0)
		FAIL("Failed to open compressed file");

	ret = compression_stream_init(&cs, &fs.base, COMPRESS_ZSTD, O_RDONLY, 1);
	if (ret < 0) {
		stream_close(&fs.base);
		FAIL("Failed to init zstd stream for reading");
	}

	/* Read decompressed data */
	char buf[1024];
	ssize_t nread = stream_read(&cs.base, buf, sizeof(buf));
	if (nread < 0) {
		stream_close(&cs.base);
		FAIL("Read failed");
	}

	buf[nread] = '\0';

	if (strcmp(buf, test_data) != 0) {
		stream_close(&cs.base);
		FAIL("Decompressed data doesn't match");
	}

	/* Verify we're at EOF */
	nread = stream_read(&cs.base, buf, sizeof(buf));
	if (nread != 0) {
		stream_close(&cs.base);
		FAIL("Should be at EOF");
	}

	stream_close(&cs.base);
	unlink(TEST_FILE_ZST);

	PASS();
}

/* Test auto-detection of zstd format */
void test_zstd_auto_detect(void)
{
	TEST("zstd_auto_detect");

	const char *test_data = "Auto-detection test data for zstd";

	/* Create a zstd file */
	struct file_stream fs;
	int ret = file_stream_open(&fs, TEST_FILE_ZST,
				   O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (ret < 0)
		FAIL("Failed to create test file");

	struct compression_stream cs;
	ret = compression_stream_init(&cs, &fs.base, COMPRESS_ZSTD, O_WRONLY, 1);
	if (ret < 0) {
		stream_close(&fs.base);
		FAIL("Failed to init zstd stream");
	}

	stream_write(&cs.base, test_data, strlen(test_data));
	stream_close(&cs.base);

	/* Now try to auto-detect and read */
	ret = file_stream_open(&fs, TEST_FILE_ZST, O_RDONLY, 0);
	if (ret < 0)
		FAIL("Failed to open file for reading");

	ret = compression_stream_auto(&cs, &fs.base, 1);
	if (ret < 0) {
		stream_close(&fs.base);
		FAIL("Auto-detection failed");
	}

	/* Read and verify */
	char buf[1024];
	ssize_t nread = stream_read(&cs.base, buf, sizeof(buf));
	if (nread < 0) {
		stream_close(&cs.base);
		FAIL("Read failed");
	}

	buf[nread] = '\0';

	if (strcmp(buf, test_data) != 0) {
		stream_close(&cs.base);
		FAIL("Data mismatch");
	}

	stream_close(&cs.base);
	unlink(TEST_FILE_ZST);

	PASS();
}

/* Test zstd round-trip compression/decompression */
void test_zstd_roundtrip(void)
{
	TEST("zstd_roundtrip");

	const char *test_data = "The quick brown fox jumps over the lazy dog. "
				"This sentence repeats with zstd. "
				"The quick brown fox jumps over the lazy dog. "
				"This sentence repeats with zstd. "
				"The quick brown fox jumps over the lazy dog.";

	/* Compress to memory stream */
	struct mem_stream ms_out;
	mem_stream_create(&ms_out, 0);

	struct compression_stream cs_write;
	int ret = compression_stream_init(&cs_write, &ms_out.base, COMPRESS_ZSTD,
					  O_WRONLY, 0);
	if (ret < 0) {
		stream_close(&ms_out.base);
		FAIL("Failed to init compression");
	}

	ssize_t written = stream_write(&cs_write.base, test_data, strlen(test_data));
	if (written != (ssize_t)strlen(test_data)) {
		stream_close(&cs_write.base);
		stream_close(&ms_out.base);
		FAIL("Write failed");
	}

	stream_close(&cs_write.base);

	/* Get compressed data */
	size_t compressed_size;
	const void *compressed_data = mem_stream_get_buffer(&ms_out, &compressed_size);

	/* Decompress from memory stream */
	struct mem_stream ms_in;
	mem_stream_init(&ms_in, (void *)compressed_data, compressed_size, 0);

	struct compression_stream cs_read;
	ret = compression_stream_init(&cs_read, &ms_in.base, COMPRESS_ZSTD,
				      O_RDONLY, 0);
	if (ret < 0) {
		stream_close(&ms_out.base);
		stream_close(&ms_in.base);
		FAIL("Failed to init decompression");
	}

	/* Read back */
	char buf[1024];
	ssize_t nread = stream_read(&cs_read.base, buf, sizeof(buf));
	if (nread < 0) {
		stream_close(&cs_read.base);
		stream_close(&ms_out.base);
		stream_close(&ms_in.base);
		FAIL("Read failed");
	}

	buf[nread] = '\0';

	if (strcmp(buf, test_data) != 0) {
		stream_close(&cs_read.base);
		stream_close(&ms_out.base);
		stream_close(&ms_in.base);
		FAIL("Data mismatch");
	}

	stream_close(&cs_read.base);
	stream_close(&ms_out.base);
	stream_close(&ms_in.base);

	PASS();
}

#endif /* STREAMIO_HAVE_ZSTD */

int main(void)
{
	printf("StreamIO Compression Tests\n");
	printf("===========================\n\n");

	printf("Version: %s\n", stream_get_version());
	printf("Features: %s\n\n", stream_get_features_string());

#ifdef STREAMIO_HAVE_ZLIB
	test_compression_available();
	test_gzip_write();
	test_gzip_read();
	test_gzip_auto_detect();
	test_gzip_roundtrip();
#else
	printf("SKIP: gzip tests (zlib not available)\n");
#endif

#ifdef STREAMIO_HAVE_BZIP2
	test_bzip2_available();
	test_bzip2_write();
	test_bzip2_read();
	test_bzip2_auto_detect();
	test_bzip2_roundtrip();
#else
	printf("SKIP: bzip2 tests (bzip2 not available)\n");
#endif

#ifdef STREAMIO_HAVE_LZMA
	test_xz_available();
	test_xz_write();
	test_xz_read();
	test_xz_auto_detect();
	test_xz_roundtrip();
#else
	printf("SKIP: xz tests (lzma not available)\n");
#endif

#ifdef STREAMIO_HAVE_ZSTD
	test_zstd_available();
	test_zstd_write();
	test_zstd_read();
	test_zstd_auto_detect();
	test_zstd_roundtrip();
#else
	printf("SKIP: zstd tests (zstd not available)\n");
#endif

	printf("\n===========================\n");
	printf("Tests: %d/%d passed\n", test_passed, test_count);

	return (test_passed == test_count) ? 0 : 1;
}
