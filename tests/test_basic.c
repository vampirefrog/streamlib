/**
 * @file test_basic.c
 * @brief Basic tests for streamio library
 */

#include <stream.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#ifndef _WIN32
#include <sys/mman.h>
#endif

#ifdef _WIN32
#include <windows.h>
static char g_test_file[MAX_PATH];
static const char *TEST_FILE(void) {
	static int initialized = 0;
	if (!initialized) {
		GetTempPathA(MAX_PATH, g_test_file);
		strcat(g_test_file, "streamio_test.dat");
		initialized = 1;
	}
	return g_test_file;
}
#define TEST_FILE TEST_FILE()
#else
#define TEST_FILE "/tmp/streamio_test.dat"
#endif

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

/* Test feature detection */
void test_feature_detection(void)
{
	TEST("feature_detection");

	const char *version = stream_get_version();
	assert(version != NULL);

	const char *features = stream_get_features_string();
	assert(features != NULL);

	unsigned int feature_flags = stream_get_features();
	(void)feature_flags;  /* May be 0 in minimal build */

	PASS();
}

/* Test memory stream read/write */
void test_mem_stream_basic(void)
{
	TEST("mem_stream_basic");

	struct mem_stream ms;
	int ret = mem_stream_init_dynamic(&ms, 0);
	if (ret < 0)
		FAIL("Failed to create memory stream");

	/* Write some data */
	const char *test_data = "Hello, World!";
	ssize_t written = stream_write(&ms.base, test_data, strlen(test_data));
	if (written != (ssize_t)strlen(test_data))
		FAIL("Write failed");

	/* Seek back to start */
	off64_t pos = stream_seek(&ms.base, 0, SEEK_SET);
	if (pos != 0)
		FAIL("Seek failed");

	/* Read it back */
	char buf[100];
	ssize_t nread = stream_read(&ms.base, buf, sizeof(buf));
	if (nread != (ssize_t)strlen(test_data))
		FAIL("Read failed");

	buf[nread] = '\0';
	if (strcmp(buf, test_data) != 0)
		FAIL("Data mismatch");

	/* Test size */
	off64_t size = stream_size(&ms.base);
	if (size != (off64_t)strlen(test_data))
		FAIL("Size mismatch");

	stream_close(&ms.base);
	PASS();
}

/* Test memory stream from existing buffer */
void test_mem_stream_existing(void)
{
	TEST("mem_stream_existing");

	char data[] = "Test data";
	struct mem_stream ms;

	int ret = mem_stream_init(&ms, data, strlen(data), 1);
	if (ret < 0)
		FAIL("Failed to init memory stream");

	/* Read the data */
	char buf[100];
	ssize_t nread = stream_read(&ms.base, buf, sizeof(buf));
	if (nread != (ssize_t)strlen(data))
		FAIL("Read failed");

	buf[nread] = '\0';
	if (strcmp(buf, data) != 0)
		FAIL("Data mismatch");

	stream_close(&ms.base);
	PASS();
}

/* Test file stream write and read */
void test_file_stream_basic(void)
{
	TEST("file_stream_basic");

	struct file_stream fs;
	const char *test_data = "File stream test data";

	/* Write to file */
	int ret = file_stream_open(&fs, TEST_FILE,
				   O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (ret < 0)
		FAIL("Failed to open file for writing");

	ssize_t written = stream_write(&fs.base, test_data, strlen(test_data));
	if (written != (ssize_t)strlen(test_data))
		FAIL("Write failed");

	stream_close(&fs.base);

	/* Read from file */
	ret = file_stream_open(&fs, TEST_FILE, O_RDONLY, 0);
	if (ret < 0)
		FAIL("Failed to open file for reading");

	char buf[100];
	ssize_t nread = stream_read(&fs.base, buf, sizeof(buf));
	if (nread != (ssize_t)strlen(test_data))
		FAIL("Read failed");

	buf[nread] = '\0';
	if (strcmp(buf, test_data) != 0)
		FAIL("Data mismatch");

	off64_t size = stream_size(&fs.base);
	if (size != (off64_t)strlen(test_data))
		FAIL("Size mismatch");

	stream_close(&fs.base);
	unlink(TEST_FILE);

	PASS();
}

/* Test file stream seeking */
void test_file_stream_seek(void)
{
	TEST("file_stream_seek");

	struct file_stream fs;
	const char *test_data = "0123456789";

	/* Create test file */
	int ret = file_stream_open(&fs, TEST_FILE,
				   O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (ret < 0)
		FAIL("Failed to create file");

	stream_write(&fs.base, test_data, strlen(test_data));
	stream_close(&fs.base);

	/* Open for reading */
	ret = file_stream_open(&fs, TEST_FILE, O_RDONLY, 0);
	if (ret < 0)
		FAIL("Failed to open file");

	/* Seek to position 5 */
	off64_t pos = stream_seek(&fs.base, 5, SEEK_SET);
	if (pos != 5)
		FAIL("SEEK_SET failed");

	char buf[10];
	ssize_t nread = stream_read(&fs.base, buf, 1);
	if (nread != 1 || buf[0] != '5')
		FAIL("Read after SEEK_SET failed");

	/* Seek forward */
	pos = stream_seek(&fs.base, 2, SEEK_CUR);
	if (pos != 8)
		FAIL("SEEK_CUR failed");

	nread = stream_read(&fs.base, buf, 1);
	if (nread != 1 || buf[0] != '8')
		FAIL("Read after SEEK_CUR failed");

	/* Seek from end */
	pos = stream_seek(&fs.base, -3, SEEK_END);
	if (pos != 7)
		FAIL("SEEK_END failed");

	nread = stream_read(&fs.base, buf, 1);
	if (nread != 1 || buf[0] != '7')
		FAIL("Read after SEEK_END failed");

	stream_close(&fs.base);
	unlink(TEST_FILE);

	PASS();
}

/* Test mmap on file stream */
void test_file_stream_mmap(void)
{
	TEST("file_stream_mmap");

	struct file_stream fs;
	const char *test_data = "mmap test data";

	/* Create test file */
	int ret = file_stream_open(&fs, TEST_FILE,
				   O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (ret < 0)
		FAIL("Failed to create file");

	stream_write(&fs.base, test_data, strlen(test_data));
	stream_close(&fs.base);

	/* Open for reading with mmap */
	ret = file_stream_open(&fs, TEST_FILE, O_RDONLY, 0);
	if (ret < 0)
		FAIL("Failed to open file");

	/* Map the file */
	void *addr = stream_mmap(&fs.base, 0, strlen(test_data), PROT_READ);
	if (!addr)
		FAIL("mmap failed");

	/* Verify data */
	if (memcmp(addr, test_data, strlen(test_data)) != 0)
		FAIL("mmap data mismatch");

	/* Unmap */
	ret = stream_munmap(&fs.base, addr, strlen(test_data));
	if (ret < 0)
		FAIL("munmap failed");

	stream_close(&fs.base);
	unlink(TEST_FILE);

	PASS();
}

/* Test capability queries */
void test_capabilities(void)
{
	TEST("capabilities");

	struct mem_stream ms;
	mem_stream_init_dynamic(&ms, 0);

	assert(stream_can_read(&ms.base));
	assert(stream_can_write(&ms.base));
	assert(stream_can_seek(&ms.base));
	assert(stream_can_mmap(&ms.base));

	stream_close(&ms.base);

	PASS();
}

int main(void)
{
	printf("StreamIO Basic Tests\n");
	printf("====================\n\n");

	printf("Version: %s\n", stream_get_version());
	printf("Features: %s\n\n", stream_get_features_string());

	test_feature_detection();
	test_mem_stream_basic();
	test_mem_stream_existing();
	test_file_stream_basic();
	test_file_stream_seek();
	test_file_stream_mmap();
	test_capabilities();

	printf("\n====================\n");
	printf("Tests: %d/%d passed\n", test_passed, test_count);

	return (test_passed == test_count) ? 0 : 1;
}
