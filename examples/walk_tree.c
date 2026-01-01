/**
 * @file walk_tree.c
 * @brief Example of using the path walker
 */

#include <stream.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct walk_options {
	int count;
	int show_content;
};

static int entry_callback(const struct walker_entry *entry, void *userdata)
{
	struct walk_options *opts = userdata;
	opts->count++;

	/* Print indentation based on depth */
	for (int i = 0; i < entry->depth; i++)
		printf("  ");

	/* Print entry info */
	if (entry->is_archive_entry)
		printf("[AR] ");
	else if (entry->is_dir)
		printf("[DIR] ");
	else
		printf("[FILE] ");

	printf("%s", entry->name);

	if (!entry->is_dir)
		printf(" (%lld bytes)", (long long)entry->size);

	printf("\n");

	/* Optionally show file content */
	if (opts->show_content && !entry->is_dir && entry->stream) {
		char buf[256];
		ssize_t nread = stream_read(entry->stream, buf, sizeof(buf) - 1);
		if (nread > 0) {
			buf[nread] = '\0';
			/* Print content with extra indentation */
			for (int i = 0; i <= entry->depth; i++)
				printf("  ");
			printf("Content: \"%s\"%s\n", buf, nread == sizeof(buf) - 1 ? "..." : "");
		}
	}

	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		printf("Usage: %s <path> [options]\n", argv[0]);
		printf("\nOptions:\n");
		printf("  --recurse         Recurse into subdirectories\n");
		printf("  --expand-archives Expand archive contents\n");
		printf("  --decompress      Auto-decompress compressed files\n");
		printf("  --files-only      Show only files\n");
		printf("  --dirs-only       Show only directories\n");
		printf("  --show-content    Display file contents\n");
		printf("\nExamples:\n");
		printf("  %s /tmp\n", argv[0]);
		printf("  %s /tmp --recurse\n", argv[0]);
		printf("  %s archive.tar --expand-archives --show-content\n", argv[0]);
		printf("  %s file.gz --decompress --show-content\n", argv[0]);
		return 1;
	}

	const char *path = argv[1];
	unsigned int flags = 0;
	struct walk_options opts = { 0, 0 };

	/* Parse options */
	for (int i = 2; i < argc; i++) {
		if (strcmp(argv[i], "--recurse") == 0)
			flags |= WALK_RECURSE_DIRS;
		else if (strcmp(argv[i], "--expand-archives") == 0)
			flags |= WALK_EXPAND_ARCHIVES;
		else if (strcmp(argv[i], "--decompress") == 0)
			flags |= WALK_DECOMPRESS;
		else if (strcmp(argv[i], "--files-only") == 0)
			flags |= WALK_FILTER_FILES;
		else if (strcmp(argv[i], "--dirs-only") == 0)
			flags |= WALK_FILTER_DIRS;
		else if (strcmp(argv[i], "--show-content") == 0)
			opts.show_content = 1;
		else {
			fprintf(stderr, "Unknown option: %s\n", argv[i]);
			return 1;
		}
	}

	printf("Walking: %s\n", path);
	printf("Features: %s\n\n", stream_get_features_string());

	int ret = walk_path(path, entry_callback, &opts, flags);
	if (ret < 0) {
		fprintf(stderr, "Error walking path: %s (error %d)\n", path, ret);
		return 1;
	}

	printf("\nTotal entries: %d\n", opts.count);

	return 0;
}
