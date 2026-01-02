/**
 * @file test_walker.c
 * @brief Tests for path walker
 */

#include <stream.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <direct.h>  /* For _mkdir on Windows */
#endif
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
static char g_test_dir[MAX_PATH];
static const char *get_test_dir(void) {
	static int initialized = 0;
	if (!initialized) {
		GetTempPathA(MAX_PATH, g_test_dir);
		strcat(g_test_dir, "streamio_walker_test");
		initialized = 1;
	}
	return g_test_dir;
}
#define TEST_DIR get_test_dir()
#else
#define TEST_DIR "/tmp/streamio_walker_test"
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

/* Helper to create directory cross-platform */
static int mkdir_compat(const char *path) {
#ifdef _WIN32
	return _mkdir(path);
#else
	return mkdir(path, 0755);
#endif
}

/* Helper to build path cross-platform */
static void build_path(char *dest, size_t size, const char *dir, const char *file) {
#ifdef _WIN32
	snprintf(dest, size, "%s\\%s", dir, file);
#else
	snprintf(dest, size, "%s/%s", dir, file);
#endif
}

/* Recursive directory removal */
static void rmdir_recursive(const char *path) {
#ifdef _WIN32
	char cmd[MAX_PATH + 20];
	snprintf(cmd, sizeof(cmd), "rmdir /s /q \"%s\" 2>nul", path);
	system(cmd);
#else
	char cmd[PATH_MAX + 20];
	snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", path);
	system(cmd);
#endif
}

/* Create test directory structure */
static int create_test_tree(void)
{
	char path[512];

	rmdir_recursive(TEST_DIR);
	mkdir_compat(TEST_DIR);

	/* Create files */
	build_path(path, sizeof(path), TEST_DIR, "file1.txt");
	FILE *f1 = fopen(path, "w");
	if (!f1) return -1;
	fprintf(f1, "File 1 content");
	fclose(f1);

	build_path(path, sizeof(path), TEST_DIR, "file2.txt");
	FILE *f2 = fopen(path, "w");
	if (!f2) return -1;
	fprintf(f2, "File 2 content");
	fclose(f2);

	/* Create subdirectory */
	build_path(path, sizeof(path), TEST_DIR, "subdir");
	mkdir_compat(path);

	char subdir_path[512];
	build_path(subdir_path, sizeof(subdir_path), TEST_DIR, "subdir");
	build_path(path, sizeof(path), subdir_path, "file3.txt");
	FILE *f3 = fopen(path, "w");
	if (!f3) return -1;
	fprintf(f3, "File 3 in subdir");
	fclose(f3);

	/* Create nested subdirectory */
	char nested_path[512];
	build_path(nested_path, sizeof(nested_path), subdir_path, "nested");
	mkdir_compat(nested_path);

	build_path(path, sizeof(path), nested_path, "file4.txt");
	FILE *f4 = fopen(path, "w");
	if (!f4) return -1;
	fprintf(f4, "File 4 in nested");
	fclose(f4);

	return 0;
}

/* Clean up test files */
static void cleanup_test_tree(void)
{
	rmdir_recursive(TEST_DIR);
}

/* Test walking a single file */
static int single_file_count = 0;
static int single_file_callback(const struct walker_entry *entry, void *userdata)
{
	(void)userdata;
	single_file_count++;
	return 0;
}

void test_walk_single_file(void)
{
	TEST("walk_single_file");

	if (create_test_tree() != 0)
		FAIL("Failed to create test tree");

	char file_path[512];
	build_path(file_path, sizeof(file_path), TEST_DIR, "file1.txt");

	single_file_count = 0;
	int ret = walk_path(file_path, single_file_callback, NULL, 0);
	if (ret < 0) {
		cleanup_test_tree();
		FAIL("walk_path failed");
	}

	if (single_file_count != 1) {
		cleanup_test_tree();
		FAIL("Should have found exactly 1 entry");
	}

	cleanup_test_tree();
	PASS();
}

/* Test walking a directory (non-recursive) */
static int dir_count = 0;
static int dir_callback(const struct walker_entry *entry, void *userdata)
{
	(void)entry;
	(void)userdata;
	dir_count++;
	return 0;
}

void test_walk_directory_nonrecursive(void)
{
	TEST("walk_directory_nonrecursive");

	if (create_test_tree() != 0)
		FAIL("Failed to create test tree");

	dir_count = 0;
	int ret = walk_path(TEST_DIR, dir_callback, NULL, 0);
	if (ret < 0) {
		cleanup_test_tree();
		FAIL("walk_path failed");
	}

	/* Should only get the directory itself */
	if (dir_count != 1) {
		printf("\n    Found %d entries, expected 1", dir_count);
		cleanup_test_tree();
		FAIL("Wrong number of entries");
	}

	cleanup_test_tree();
	PASS();
}

/* Test recursive directory walking */
static int recursive_count = 0;
static int recursive_files = 0;
static int recursive_dirs = 0;

static int recursive_callback(const struct walker_entry *entry, void *userdata)
{
	(void)userdata;
	recursive_count++;
	if (entry->is_dir)
		recursive_dirs++;
	else
		recursive_files++;

	printf("\n    [depth=%d] %s %s",
	       entry->depth, entry->is_dir ? "DIR " : "FILE", entry->name);

	return 0;
}

void test_walk_directory_recursive(void)
{
	TEST("walk_directory_recursive");

	if (create_test_tree() != 0)
		FAIL("Failed to create test tree");

	recursive_count = 0;
	recursive_files = 0;
	recursive_dirs = 0;

	int ret = walk_path(TEST_DIR, recursive_callback, NULL, WALK_RECURSE_DIRS);
	if (ret < 0) {
		cleanup_test_tree();
		FAIL("walk_path failed");
	}

	printf("\n    Total: %d entries (%d files, %d dirs)",
	       recursive_count, recursive_files, recursive_dirs);

	/* Should find: TEST_DIR + subdir + nested + 4 files = 7 entries */
	if (recursive_count < 7) {
		cleanup_test_tree();
		FAIL("Should have found at least 7 entries");
	}

	if (recursive_files != 4) {
		cleanup_test_tree();
		FAIL("Should have found exactly 4 files");
	}

	cleanup_test_tree();
	PASS();
}

/* Test filtering files only */
static int files_only_count = 0;

static int files_only_callback(const struct walker_entry *entry, void *userdata)
{
	(void)userdata;
	if (entry->is_dir) {
		printf("\n    ERROR: Found directory: %s", entry->name);
		return -1;
	}
	files_only_count++;
	return 0;
}

void test_walk_filter_files(void)
{
	TEST("walk_filter_files");

	if (create_test_tree() != 0)
		FAIL("Failed to create test tree");

	files_only_count = 0;
	int ret = walk_path(TEST_DIR, files_only_callback, NULL,
			    WALK_RECURSE_DIRS | WALK_FILTER_FILES);
	if (ret < 0) {
		cleanup_test_tree();
		FAIL("walk_path failed or found a directory");
	}

	if (files_only_count != 4) {
		printf("\n    Found %d files, expected 4", files_only_count);
		cleanup_test_tree();
		FAIL("Wrong number of files");
	}

	cleanup_test_tree();
	PASS();
}

/* Test filtering dirs only */
static int dirs_only_count = 0;

static int dirs_only_callback(const struct walker_entry *entry, void *userdata)
{
	(void)userdata;
	if (!entry->is_dir) {
		printf("\n    ERROR: Found file: %s", entry->name);
		return -1;
	}
	dirs_only_count++;
	return 0;
}

void test_walk_filter_dirs(void)
{
	TEST("walk_filter_dirs");

	if (create_test_tree() != 0)
		FAIL("Failed to create test tree");

	dirs_only_count = 0;
	int ret = walk_path(TEST_DIR, dirs_only_callback, NULL,
			    WALK_RECURSE_DIRS | WALK_FILTER_DIRS);
	if (ret < 0) {
		cleanup_test_tree();
		FAIL("walk_path failed or found a file");
	}

	/* Should find: TEST_DIR + subdir + nested = 3 directories */
	if (dirs_only_count != 3) {
		printf("\n    Found %d dirs, expected 3", dirs_only_count);
		cleanup_test_tree();
		FAIL("Wrong number of directories");
	}

	cleanup_test_tree();
	PASS();
}

/* Test reading from file streams */
static int read_stream_callback(const struct walker_entry *entry, void *userdata)
{
	(void)userdata;

	/* Only try to read regular files */
	if (entry->is_dir || !entry->stream)
		return 0;

	/* Read first 32 bytes */
	char buf[64];
	ssize_t nread = stream_read(entry->stream, buf, sizeof(buf) - 1);
	if (nread > 0) {
		buf[nread] = '\0';
		printf("\n    Read from %s: \"%s\"", entry->name, buf);
	}

	return 0;
}

void test_read_file_streams(void)
{
	TEST("read_file_streams");

	if (create_test_tree() != 0)
		FAIL("Failed to create test tree");

	int ret = walk_path(TEST_DIR, read_stream_callback, NULL,
			    WALK_RECURSE_DIRS | WALK_FILTER_FILES);
	if (ret < 0) {
		cleanup_test_tree();
		FAIL("walk_path failed");
	}

	cleanup_test_tree();
	PASS();
}

#ifdef HAVE_LIBARCHIVE
/* Test archive expansion */
static int archive_entry_count = 0;
static int archive_callback(const struct walker_entry *entry, void *userdata)
{
	(void)userdata;
	archive_entry_count++;
	printf("\n    %s %s (archive=%d)",
	       entry->is_dir ? "DIR " : "FILE",
	       entry->name,
	       entry->is_archive_entry);
	return 0;
}

void test_walk_expand_archive(void)
{
	TEST("walk_expand_archive");

	if (create_test_tree() != 0)
		FAIL("Failed to create test tree");

	/* Create a tar archive */
	char cmd[512];
	snprintf(cmd, sizeof(cmd),
		 "cd /tmp && tar -cf streamio_walker_test/test.tar streamio_walker_test/*.txt 2>/dev/null");
	system(cmd);

	archive_entry_count = 0;
	int ret = walk_path(TEST_DIR, archive_callback, NULL,
			    WALK_RECURSE_DIRS | WALK_EXPAND_ARCHIVES);
	if (ret < 0) {
		cleanup_test_tree();
		FAIL("walk_path failed");
	}

	printf("\n    Total entries (including archive contents): %d",
	       archive_entry_count);

	/* Should find filesystem entries + archive entries */
	/* We should have at least the original files plus archive entries */
	if (archive_entry_count < 7) {
		cleanup_test_tree();
		FAIL("Should have found entries in archive");
	}

	cleanup_test_tree();
	PASS();
}

/* Test reading from archive entry streams */
static int read_archive_callback(const struct walker_entry *entry, void *userdata)
{
	(void)userdata;

	/* Only try to read archive files */
	if (entry->is_dir || !entry->stream || !entry->is_archive_entry)
		return 0;

	/* Read entire content */
	char buf[256];
	ssize_t nread = stream_read(entry->stream, buf, sizeof(buf) - 1);
	if (nread > 0) {
		buf[nread] = '\0';
		printf("\n    Archive entry %s: \"%s\"", entry->name, buf);
	}

	return 0;
}

void test_read_archive_streams(void)
{
	TEST("read_archive_streams");

	if (create_test_tree() != 0)
		FAIL("Failed to create test tree");

	/* Create a tar archive */
	char cmd[512];
	snprintf(cmd, sizeof(cmd),
		 "cd /tmp && tar -cf streamio_walker_test/test.tar streamio_walker_test/*.txt 2>/dev/null");
	system(cmd);

	int ret = walk_path(TEST_DIR, read_archive_callback, NULL,
			    WALK_RECURSE_DIRS | WALK_EXPAND_ARCHIVES);
	if (ret < 0) {
		cleanup_test_tree();
		FAIL("walk_path failed");
	}

	cleanup_test_tree();
	PASS();
}

#ifdef HAVE_ZLIB
/* Test decompression while walking */
void test_walk_decompress(void)
{
	TEST("walk_decompress");

	if (create_test_tree() != 0)
		FAIL("Failed to create test tree");

	/* Create a compressed tar archive */
	char cmd[512];
	snprintf(cmd, sizeof(cmd),
		 "cd /tmp && tar czf streamio_walker_test/test.tar.gz streamio_walker_test/*.txt 2>/dev/null");
	system(cmd);

	/* Walk and read from compressed archive */
	int ret = walk_path(TEST_DIR "/test.tar.gz", read_archive_callback, NULL,
			    WALK_EXPAND_ARCHIVES | WALK_DECOMPRESS);
	if (ret < 0) {
		cleanup_test_tree();
		FAIL("walk_path with decompression failed");
	}

	cleanup_test_tree();
	PASS();
}
#endif
#endif

int main(void)
{
	printf("StreamIO Walker Tests\n");
	printf("======================\n\n");

	printf("Version: %s\n", stream_get_version());
	printf("Features: %s\n\n", stream_get_features_string());

	test_walk_single_file();
	test_walk_directory_nonrecursive();
	test_walk_directory_recursive();
	test_walk_filter_files();
	test_walk_filter_dirs();
	test_read_file_streams();

#if defined(HAVE_LIBARCHIVE) && !defined(_WIN32)
	/* Archive tests require tar command, skip on Windows */
	test_walk_expand_archive();
	test_read_archive_streams();
#ifdef HAVE_ZLIB
	test_walk_decompress();
#endif
#endif

	printf("\n======================\n");
	printf("Tests: %d/%d passed\n", test_passed, test_count);

	return (test_passed == test_count) ? 0 : 1;
}
