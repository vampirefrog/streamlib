#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <zip.h>
#include "../stream.h"

// Helper to create a zip file with a single file "hello.txt" containing "hello world"
static void create_test_zip(const char *zipname, const char *filename, const char *contents) {
	int err = 0;
	zip_t *zip = zip_open(zipname, ZIP_CREATE | ZIP_TRUNCATE, &err);
	assert(zip);
	zip_source_t *src = zip_source_buffer(zip, contents, strlen(contents), 0);
	assert(src);
	assert(zip_file_add(zip, filename, src, ZIP_FL_OVERWRITE) >= 0);
	assert(zip_close(zip) == 0);
}

static void remove_test_zip(const char *zipname) {
	remove(zipname);
}

int main(void) {
#ifdef HAVE_LIBZIP
	const char *zipname = "test.zip";
	const char *filename = "hello.txt";
	const char *expected = "hello world";
	char buf[64] = {0};

	create_test_zip(zipname, filename, expected);

	int err = 0;
	zip_t *zip = zip_open(zipname, 0, &err);
	assert(zip);

	int idx = zip_name_locate(zip, filename, 0);
	assert(idx >= 0);

	struct stream *s = zip_file_stream_create_index(zip, idx, STREAM_ENSURE_MMAP);
	assert(s);

	ssize_t n = s->read(s, buf, sizeof(buf)-1);
	assert(n == (ssize_t)strlen(expected));
	assert(memcmp(buf, expected, strlen(expected)) == 0);

	// Seek and re-read
	assert(s->seek(s, 0, SEEK_SET) == 0);
	memset(buf, 0, sizeof(buf));
	n = s->read(s, buf, 5);
	assert(n == 5);
	assert(memcmp(buf, "hello", 5) == 0);

	s->close(s);
	zip_close(zip);

	remove_test_zip(zipname);

	printf("zip_file_stream mmap test passed\n");
#else
	printf("libzip not available, skipping test\n");
#endif
	return 0;
}
