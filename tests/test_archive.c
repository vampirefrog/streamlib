/**
 * @file test_archive.c
 * @brief Tests for archive support
 */

#include <stream.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
static char g_temp_path[MAX_PATH];
static char g_test_tar[MAX_PATH];
static char g_test_tgz[MAX_PATH];
static char g_test_dir[MAX_PATH];

static void init_test_paths(void) {
	GetTempPathA(MAX_PATH, g_temp_path);
	snprintf(g_test_tar, MAX_PATH, "%sstreamio_test.tar", g_temp_path);
	snprintf(g_test_tgz, MAX_PATH, "%sstreamio_test.tar.gz", g_temp_path);
	snprintf(g_test_dir, MAX_PATH, "%sstreamio_test_dir", g_temp_path);
}
#define TEST_TAR g_test_tar
#define TEST_TGZ g_test_tgz
#define TEST_DIR g_test_dir
#else
#define TEST_TAR "/tmp/streamio_test.tar"
#define TEST_TGZ "/tmp/streamio_test.tar.gz"
#define TEST_DIR "/tmp/streamio_test_dir"
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

#ifdef HAVE_LIBARCHIVE
#include <archive.h>
#include <archive_entry.h>

/* Create a test tar file using system tar command */
static int create_test_tar(void)
{
	char path[512];

	/* Create test directory and files */
#ifdef _WIN32
	_mkdir(TEST_DIR);
#else
	mkdir(TEST_DIR, 0755);
#endif

	snprintf(path, sizeof(path), "%s/file1.txt", TEST_DIR);
	FILE *f1 = fopen(path, "w");
	if (!f1)
		return -1;
	fprintf(f1, "Content of file 1");
	fclose(f1);

	snprintf(path, sizeof(path), "%s/file2.txt", TEST_DIR);
	FILE *f2 = fopen(path, "w");
	if (!f2)
		return -1;
	fprintf(f2, "Content of file 2");
	fclose(f2);

	snprintf(path, sizeof(path), "%s/subdir", TEST_DIR);
#ifdef _WIN32
	_mkdir(path);
#else
	mkdir(path, 0755);
#endif

	snprintf(path, sizeof(path), "%s/subdir/file3.txt", TEST_DIR);
	FILE *f3 = fopen(path, "w");
	if (!f3)
		return -1;
	fprintf(f3, "Content of file 3 in subdir");
	fclose(f3);

	/* Create tar archive */
	char cmd[512];
	snprintf(cmd, sizeof(cmd), "cd /tmp && tar -cf streamio_test.tar streamio_test_dir");
	int ret = system(cmd);

	return ret;
}

/* Clean up test files */
static void cleanup_test_files(void)
{
#ifdef _WIN32
	char cmd[512];
	snprintf(cmd, sizeof(cmd), "rmdir /s /q \"%s\" 2>nul", TEST_DIR);
	system(cmd);
#else
	char cmd[512];
	snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", TEST_DIR);
	system(cmd);
#endif
	unlink(TEST_TAR);
	unlink(TEST_TGZ);
}

/* Test archive feature detection */
void test_archive_available(void)
{
	TEST("archive_available");

	if (!stream_has_feature(STREAM_FEAT_LIBARCHIVE))
		FAIL("LIBARCHIVE feature should be available");

	PASS();
}

/* Test opening a tar archive */
void test_archive_open(void)
{
	TEST("archive_open");

	if (create_test_tar() != 0)
		FAIL("Failed to create test tar");

	/* Open tar file */
	struct file_stream fs;
	int ret = file_stream_open(&fs, TEST_TAR, O_RDONLY, 0);
	if (ret < 0) {
		cleanup_test_files();
		FAIL("Failed to open tar file");
	}

	/* Open as archive */
	struct archive_stream ar;
	ret = archive_stream_open(&ar, &fs.base, 1);
	if (ret < 0) {
		stream_close(&fs.base);
		cleanup_test_files();
		FAIL("Failed to open archive stream");
	}

	/* Close */
	archive_stream_close(&ar);
	cleanup_test_files();

	PASS();
}

/* Walker callback for counting entries */
static int entry_count = 0;
static int count_entries_callback(const struct archive_entry_info *entry,
				   void *userdata)
{
	(void)userdata;
	entry_count++;
	printf("\n    Found: %s (%s, %lld bytes)",
	       entry->pathname,
	       entry->is_dir ? "dir" : "file",
	       (long long)entry->size);
	return 0;
}

/* Test walking through archive entries */
void test_archive_walk(void)
{
	TEST("archive_walk");

	if (create_test_tar() != 0)
		FAIL("Failed to create test tar");

	struct file_stream fs;
	int ret = file_stream_open(&fs, TEST_TAR, O_RDONLY, 0);
	if (ret < 0) {
		cleanup_test_files();
		FAIL("Failed to open tar file");
	}

	struct archive_stream ar;
	ret = archive_stream_open(&ar, &fs.base, 1);
	if (ret < 0) {
		stream_close(&fs.base);
		cleanup_test_files();
		FAIL("Failed to open archive stream");
	}

	/* Walk entries */
	entry_count = 0;
	ret = archive_stream_walk(&ar, count_entries_callback, NULL);
	if (ret < 0) {
		archive_stream_close(&ar);
		cleanup_test_files();
		FAIL("Failed to walk archive");
	}

	printf("\n    Total entries: %d", entry_count);

	/* Should have at least 4 entries (dir, 2 files, subdir, 1 file in subdir) */
	if (entry_count < 4) {
		archive_stream_close(&ar);
		cleanup_test_files();
		FAIL("Not enough entries found");
	}

	archive_stream_close(&ar);
	cleanup_test_files();

	PASS();
}

/* Test reading specific entry data */
struct find_entry_data {
	const char *name;
	int found;
	char content[1024];
};

static int find_entry_callback(const struct archive_entry_info *entry,
				void *userdata)
{
	struct find_entry_data *data = userdata;

	if (strstr(entry->pathname, data->name) != NULL && !entry->is_dir) {
		data->found = 1;
		return 1;  /* Stop iteration */
	}

	return 0;
}

void test_archive_read_entry(void)
{
	TEST("archive_read_entry");

	if (create_test_tar() != 0)
		FAIL("Failed to create test tar");

	struct file_stream fs;
	int ret = file_stream_open(&fs, TEST_TAR, O_RDONLY, 0);
	if (ret < 0) {
		cleanup_test_files();
		FAIL("Failed to open tar file");
	}

	struct archive_stream ar;
	ret = archive_stream_open(&ar, &fs.base, 1);
	if (ret < 0) {
		stream_close(&fs.base);
		cleanup_test_files();
		FAIL("Failed to open archive stream");
	}

	/* Walk until we find file1.txt and read it during the walk */
	struct find_entry_data data;
	data.name = "file1.txt";
	data.found = 0;
	memset(data.content, 0, sizeof(data.content));

	/* We need to read the entry data during the walk, while positioned at that entry */
	struct archive *a = ar.archive;
	struct archive_entry *entry;
	int found = 0;

	while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
		const char *pathname = archive_entry_pathname(entry);
		if (strstr(pathname, "file1.txt") != NULL) {
			/* Read the data while we're positioned at this entry */
			la_ssize_t nread = archive_read_data(a, data.content,
							     sizeof(data.content) - 1);
			if (nread < 0) {
				archive_stream_close(&ar);
				cleanup_test_files();
				FAIL("Failed to read entry data");
			}
			data.content[nread] = '\0';
			found = 1;
			break;
		}
		/* Skip entries we don't want */
		archive_read_data_skip(a);
	}

	if (!found) {
		archive_stream_close(&ar);
		cleanup_test_files();
		FAIL("Failed to find file1.txt");
	}

	if (strcmp(data.content, "Content of file 1") != 0) {
		printf("\n    Got: '%s'", data.content);
		archive_stream_close(&ar);
		cleanup_test_files();
		FAIL("Content mismatch");
	}

	archive_stream_close(&ar);
	cleanup_test_files();

	PASS();
}

#ifdef HAVE_ZLIB
/* Test reading compressed tar.gz archive */
void test_archive_compressed(void)
{
	TEST("archive_compressed");

	if (create_test_tar() != 0)
		FAIL("Failed to create test tar");

	/* Compress the tar file */
	char cmd[512];
	snprintf(cmd, sizeof(cmd), "gzip -c %s > %s", TEST_TAR, TEST_TGZ);
	if (system(cmd) != 0) {
		cleanup_test_files();
		FAIL("Failed to compress tar");
	}

	/* Open compressed tar */
	struct file_stream fs;
	int ret = file_stream_open(&fs, TEST_TGZ, O_RDONLY, 0);
	if (ret < 0) {
		cleanup_test_files();
		FAIL("Failed to open tar.gz file");
	}

	/* Decompress */
	struct compression_stream cs;
	ret = gzip_stream_init(&cs, &fs.base, O_RDONLY, 1);
	if (ret < 0) {
		stream_close(&fs.base);
		cleanup_test_files();
		FAIL("Failed to init gzip stream");
	}

	/* Open as archive */
	struct archive_stream ar;
	ret = archive_stream_open(&ar, &cs.base, 1);
	if (ret < 0) {
		stream_close(&cs.base);
		cleanup_test_files();
		FAIL("Failed to open archive from compressed stream");
	}

	/* Walk entries */
	entry_count = 0;
	ret = archive_stream_walk(&ar, count_entries_callback, NULL);
	if (ret < 0) {
		archive_stream_close(&ar);
		cleanup_test_files();
		FAIL("Failed to walk compressed archive");
	}

	printf("\n    Total entries in tar.gz: %d", entry_count);

	if (entry_count < 4) {
		archive_stream_close(&ar);
		cleanup_test_files();
		FAIL("Not enough entries in compressed archive");
	}

	archive_stream_close(&ar);
	cleanup_test_files();

	PASS();
}
#endif /* HAVE_ZLIB */

/* Test creating a basic TAR archive */
static void test_archive_create_tar(void)
{
	TEST("archive_create_tar");

	const char *test_tar_out = "/tmp/streamio_test_create.tar";

	/* Create output file stream */
	struct file_stream fs;
	int ret = file_stream_open(&fs, test_tar_out,
				    O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (ret < 0)
		FAIL("Failed to open output file");

	/* Create archive stream */
	struct archive_stream ar;
	ret = archive_stream_open_write(&ar, &fs.base,
					 STREAMIO_ARCHIVE_TAR_PAX, 1);
	if (ret < 0) {
		stream_close(&fs.base);
		FAIL("Failed to open archive for writing");
	}

	/* Add first entry */
	const char *content1 = "This is file 1 content";
	size_t len1 = strlen(content1);
	ret = archive_stream_new_entry(&ar, "test1.txt", S_IFREG | 0644, len1);
	if (ret < 0) {
		archive_stream_close(&ar);
		FAIL("Failed to create entry 1");
	}

	ssize_t written = archive_stream_write_data(&ar, content1, len1);
	if (written != (ssize_t)len1) {
		archive_stream_close(&ar);
		FAIL("Failed to write entry 1 data");
	}

	ret = archive_stream_finish_entry(&ar);
	if (ret < 0) {
		archive_stream_close(&ar);
		FAIL("Failed to finish entry 1");
	}

	/* Add second entry */
	const char *content2 = "This is file 2 content with more text";
	size_t len2 = strlen(content2);
	ret = archive_stream_new_entry(&ar, "subdir/test2.txt", S_IFREG | 0644, len2);
	if (ret < 0) {
		archive_stream_close(&ar);
		FAIL("Failed to create entry 2");
	}

	written = archive_stream_write_data(&ar, content2, len2);
	if (written != (ssize_t)len2) {
		archive_stream_close(&ar);
		FAIL("Failed to write entry 2 data");
	}

	ret = archive_stream_finish_entry(&ar);
	if (ret < 0) {
		archive_stream_close(&ar);
		FAIL("Failed to finish entry 2");
	}

	/* Close and finalize */
	archive_stream_close(&ar);

	/* Verify archive was created */
	struct stat st;
	if (stat(test_tar_out, &st) < 0)
		FAIL("Output file not created");

	if (st.st_size == 0)
		FAIL("Output file is empty");

	/* Verify with libarchive by reading it back */
	struct file_stream fs_read;
	ret = file_stream_open(&fs_read, test_tar_out, O_RDONLY, 0);
	if (ret < 0)
		FAIL("Failed to open created archive");

	struct archive_stream ar_read;
	ret = archive_stream_open(&ar_read, &fs_read.base, 1);
	if (ret < 0) {
		stream_close(&fs_read.base);
		FAIL("Failed to open created archive for reading");
	}

	/* Count entries */
	int entry_count = 0;
	struct archive *a = ar_read.archive;
	struct archive_entry *entry;
	while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
		entry_count++;
		archive_read_data_skip(a);
	}

	archive_stream_close(&ar_read);

	if (entry_count != 2)
		FAIL("Wrong number of entries in created archive");

	/* Clean up */
	unlink(test_tar_out);

	PASS();
}

/* Test creating a ZIP archive */
static void test_archive_create_zip(void)
{
	TEST("archive_create_zip");

	const char *test_zip_out = "/tmp/streamio_test_create.zip";

	/* Create output file stream */
	struct file_stream fs;
	int ret = file_stream_open(&fs, test_zip_out,
				    O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (ret < 0)
		FAIL("Failed to open output file");

	/* Create ZIP archive stream */
	struct archive_stream ar;
	ret = archive_stream_open_write(&ar, &fs.base,
					 STREAMIO_ARCHIVE_ZIP, 1);
	if (ret < 0) {
		stream_close(&fs.base);
		FAIL("Failed to open ZIP archive for writing");
	}

	/* Add entry */
	const char *content = "ZIP file content";
	size_t len = strlen(content);
	ret = archive_stream_new_entry(&ar, "zipfile.txt", S_IFREG | 0644, len);
	if (ret < 0) {
		archive_stream_close(&ar);
		FAIL("Failed to create ZIP entry");
	}

	ssize_t written = archive_stream_write_data(&ar, content, len);
	if (written != (ssize_t)len) {
		archive_stream_close(&ar);
		FAIL("Failed to write ZIP entry data");
	}

	ret = archive_stream_finish_entry(&ar);
	if (ret < 0) {
		archive_stream_close(&ar);
		FAIL("Failed to finish ZIP entry");
	}

	/* Close and finalize */
	archive_stream_close(&ar);

	/* Verify archive was created */
	struct stat st;
	if (stat(test_zip_out, &st) < 0)
		FAIL("ZIP file not created");

	if (st.st_size == 0)
		FAIL("ZIP file is empty");

	/* Read it back */
	struct file_stream fs_read;
	ret = file_stream_open(&fs_read, test_zip_out, O_RDONLY, 0);
	if (ret < 0)
		FAIL("Failed to open created ZIP");

	struct archive_stream ar_read;
	ret = archive_stream_open(&ar_read, &fs_read.base, 1);
	if (ret < 0) {
		stream_close(&fs_read.base);
		FAIL("Failed to open created ZIP for reading");
	}

	archive_stream_close(&ar_read);

	/* Clean up */
	unlink(test_zip_out);

	PASS();
}

/* Test round-trip: create archive, read it back, verify content */
static void test_archive_roundtrip(void)
{
	TEST("archive_roundtrip");

	const char *test_archive = "/tmp/streamio_test_roundtrip.tar";
	const char *test_content = "Round-trip test content";
	const char *test_filename = "roundtrip.txt";

	/* Create archive */
	struct file_stream fs_write;
	int ret = file_stream_open(&fs_write, test_archive,
				    O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (ret < 0)
		FAIL("Failed to open output file");

	struct archive_stream ar_write;
	ret = archive_stream_open_write(&ar_write, &fs_write.base,
					 STREAMIO_ARCHIVE_TAR_PAX, 1);
	if (ret < 0) {
		stream_close(&fs_write.base);
		FAIL("Failed to open archive for writing");
	}

	size_t len = strlen(test_content);
	ret = archive_stream_new_entry(&ar_write, test_filename, S_IFREG | 0644, len);
	if (ret < 0) {
		archive_stream_close(&ar_write);
		FAIL("Failed to create entry");
	}

	archive_stream_write_data(&ar_write, test_content, len);
	archive_stream_finish_entry(&ar_write);
	archive_stream_close(&ar_write);

	/* Read archive back */
	struct file_stream fs_read;
	ret = file_stream_open(&fs_read, test_archive, O_RDONLY, 0);
	if (ret < 0)
		FAIL("Failed to open created archive");

	struct archive_stream ar_read;
	ret = archive_stream_open(&ar_read, &fs_read.base, 1);
	if (ret < 0) {
		stream_close(&fs_read.base);
		FAIL("Failed to open archive for reading");
	}

	/* Read entry and verify content */
	struct archive *a = ar_read.archive;
	struct archive_entry *entry;
	ret = archive_read_next_header(a, &entry);
	if (ret != ARCHIVE_OK) {
		archive_stream_close(&ar_read);
		FAIL("Failed to read entry header");
	}

	const char *pathname = archive_entry_pathname(entry);
	if (strcmp(pathname, test_filename) != 0) {
		archive_stream_close(&ar_read);
		FAIL("Wrong filename in archive");
	}

	/* Read content */
	char buffer[256];
	la_ssize_t nread = archive_read_data(a, buffer, sizeof(buffer));
	if (nread != (la_ssize_t)len) {
		archive_stream_close(&ar_read);
		FAIL("Wrong content length");
	}

	if (memcmp(buffer, test_content, len) != 0) {
		archive_stream_close(&ar_read);
		FAIL("Content mismatch");
	}

	archive_stream_close(&ar_read);

	/* Clean up */
	unlink(test_archive);

	PASS();
}

#endif /* HAVE_LIBARCHIVE */

int main(void)
{
#ifdef _WIN32
	init_test_paths();
#endif

	printf("StreamIO Archive Tests\n");
	printf("=======================\n\n");

	printf("Version: %s\n", stream_get_version());
	printf("Features: %s\n\n", stream_get_features_string());

#ifdef HAVE_LIBARCHIVE
	test_archive_available();
#ifndef _WIN32
	/* Archive tests require tar command, skip on Windows */
	test_archive_open();
	test_archive_walk();
	test_archive_read_entry();

#ifdef HAVE_ZLIB
	test_archive_compressed();
#endif

	/* Archive creation tests */
	test_archive_create_tar();
	test_archive_create_zip();
	test_archive_roundtrip();
#else
	printf("SKIP: Archive creation tests (tar command not available on Windows)\n");
#endif

#else
	printf("SKIP: Archive tests (libarchive not available)\n");
#endif

	printf("\n=======================\n");
	printf("Tests: %d/%d passed\n", test_passed, test_count);

	return (test_passed == test_count) ? 0 : 1;
}
