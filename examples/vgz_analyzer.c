/**
 * @file vgz_analyzer.c
 * @brief VGZ/VGM file analyzer with recursive archive support
 *
 * Demonstrates walking through directories containing ZIP archives,
 * which contain gzipped VGM files (.vgz), and analyzing them.
 *
 * VGM (Video Game Music) format stores sound chip data from retro games.
 * VGZ files are gzip-compressed VGM files.
 */

#include <stream.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <strings.h>
#else
/* Windows doesn't have strings.h, use string.h equivalents */
#define strcasecmp _stricmp
#endif
#include <stdint.h>

/* VGM header structure (first 64 bytes) */
struct vgm_header {
	char magic[4];          /* "Vgm " */
	uint32_t eof_offset;
	uint32_t version;
	uint32_t sn76489_clock;
	uint32_t ym2413_clock;
	uint32_t gd3_offset;
	uint32_t total_samples;
	uint32_t loop_offset;
	uint32_t loop_samples;
	uint32_t rate;
	/* ... more fields ... */
};

/* Statistics */
struct vgm_stats {
	int total_files;
	int vgm_files;
	int vgz_files;
	int corrupted_files;
	uint64_t total_bytes;
	uint64_t total_samples;
	double total_duration_sec;
};

/* Read 32-bit little-endian value */
static uint32_t read_le32(const uint8_t *data)
{
	return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

/* Analyze a VGM file */
static int analyze_vgm(struct stream *s, const char *path,
		       struct vgm_stats *stats)
{
	uint8_t header[64];
	ssize_t nread = stream_read(s, header, sizeof(header));

	if (nread < 64) {
		fprintf(stderr, "  [WARN] %s: Too small (%zd bytes)\n",
			path, nread);
		stats->corrupted_files++;
		return -1;
	}

	/* Check magic */
	if (memcmp(header, "Vgm ", 4) != 0) {
		fprintf(stderr, "  [WARN] %s: Invalid VGM magic (got: %02x %02x %02x %02x)\n",
			path, header[0], header[1], header[2], header[3]);
		stats->corrupted_files++;
		return -1;
	}

	/* Parse header */
	uint32_t version = read_le32(header + 8);
	uint32_t total_samples = read_le32(header + 0x18);
	uint32_t rate = read_le32(header + 0x24);
	uint32_t sn76489_clock = read_le32(header + 0x0C);
	uint32_t ym2413_clock = read_le32(header + 0x10);

	/* Calculate duration (samples at 44100 Hz) */
	double duration = total_samples / 44100.0;

	/* Detect sound chips */
	char chips[128] = "";
	if (sn76489_clock > 0)
		strcat(chips, "SN76489 ");
	if (ym2413_clock > 0)
		strcat(chips, "YM2413 ");
	if (chips[0] == '\0')
		strcpy(chips, "Unknown");

	printf("  %-50s | v%d.%02d | %6.2fs | %s\n",
	       path,
	       (version >> 8) & 0xFF, version & 0xFF,
	       duration,
	       chips);

	/* Update statistics */
	stats->total_samples += total_samples;
	stats->total_duration_sec += duration;

	return 0;
}

/* Walker callback for processing files */
static int process_file(const struct walker_entry *entry, void *userdata)
{
	struct vgm_stats *stats = userdata;

	stats->total_files++;

	/* Skip directories */
	if (entry->is_dir) {
		return 0;
	}

	/* Get file extension */
	const char *ext = strrchr(entry->name, '.');
	if (!ext)
		return 0;

	/* Check if it's a VGM or VGZ file */
	int is_vgm = (strcasecmp(ext, ".vgm") == 0);
	int is_vgz = (strcasecmp(ext, ".vgz") == 0);

	if (!is_vgm && !is_vgz)
		return 0;

	if (is_vgz)
		stats->vgz_files++;
	else
		stats->vgm_files++;

	/* Analyze the VGM data (stream is already decompressed if .vgz) */
	if (entry->stream) {
		stats->total_bytes += entry->size;
		analyze_vgm(entry->stream, entry->path, stats);
	} else {
		fprintf(stderr, "  [WARN] %s: No stream available\n", entry->path);
	}

	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		printf("Usage: %s <path> [<path> ...]\n", argv[0]);
		printf("Recursively analyzes VGM/VGZ files in directories and archives\n");
		printf("\nExample: %s /path/to/vgm/collection\n", argv[0]);
		return 1;
	}

	printf("VGM/VGZ File Analyzer\n");
	printf("=====================\n\n");

	printf("%-52s | Version | Duration | Chips\n", "File");
	printf("%s\n", "------------------------------------------------------------"
	                "------------------------------------------------------------");

	struct vgm_stats stats = {0};

	/* Process each path */
	for (int i = 1; i < argc; i++) {
		const char *path = argv[i];

		/* Walk path with:
		 * - WALK_RECURSE_DIRS: Recurse into subdirectories
		 * - WALK_EXPAND_ARCHIVES: Open ZIP/TAR files and process contents
		 * - WALK_DECOMPRESS: Auto-decompress .gz/.bz2/.xz files
		 * - WALK_FILTER_FILES: Only call callback for files, not dirs
		 */
		unsigned int flags = WALK_RECURSE_DIRS |
				     WALK_EXPAND_ARCHIVES |
				     WALK_DECOMPRESS |
				     WALK_FILTER_FILES;

		int ret = walk_path(path, process_file, &stats, flags);
		if (ret < 0) {
			fprintf(stderr, "Error walking %s: %s\n",
				path, strerror(-ret));
			continue;
		}
	}

	/* Print statistics */
	printf("\n%s\n", "------------------------------------------------------------"
	                  "------------------------------------------------------------");
	printf("Statistics:\n");
	printf("  Total files scanned:  %d\n", stats.total_files);
	printf("  VGM files:            %d\n", stats.vgm_files);
	printf("  VGZ files:            %d (gzip-compressed)\n", stats.vgz_files);
	printf("  Corrupted files:      %d\n", stats.corrupted_files);
	printf("  Total size:           %.2f MB\n",
	       stats.total_bytes / (1024.0 * 1024.0));
	printf("  Total music duration: %.2f minutes\n",
	       stats.total_duration_sec / 60.0);

	if (stats.vgm_files + stats.vgz_files > 0) {
		printf("  Average duration:     %.2f seconds\n",
		       stats.total_duration_sec / (stats.vgm_files + stats.vgz_files));
	}

	printf("\nFeatures used:\n");
	printf("  - Recursive directory traversal\n");
	printf("  - ZIP/TAR archive expansion\n");
	printf("  - Automatic gzip decompression (.vgz files)\n");
	printf("  - Streaming analysis (no temporary files)\n");

	return 0;
}
