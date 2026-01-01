/**
 * @file binary_writer.c
 * @brief Binary file writer demonstrating stream binary I/O API
 *
 * Shows how to write binary files using the stream API's built-in
 * binary I/O functions with proper endianness control.
 */

#include <stream.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Example: Write a simple game level format (little-endian) */
static int write_game_level(const char *filename)
{
	printf("Writing game level to: %s\n", filename);

	/* Open file for writing */
	struct file_stream fs;
	if (file_stream_open(&fs, filename, O_WRONLY | O_CREAT | O_TRUNC,
			     0644) < 0) {
		fprintf(stderr, "Failed to create %s\n", filename);
		return -1;
	}

	struct stream *s = &fs.base;

	/* Write file header */
	printf("  Writing header...\n");
	stream_write(s, "GLVL", 4);        /* Magic */
	stream_write_u16_le(s, 1);         /* Version */
	stream_write_u16_le(s, 0);         /* Flags */

	/* Write level metadata */
	printf("  Writing metadata...\n");
	stream_write_string(s, "Level 1: The Beginning");
	stream_write_u32_le(s, 1920);      /* Width */
	stream_write_u32_le(s, 1080);      /* Height */
	stream_write_u8(s, 3);             /* Difficulty */

	/* Write entity list */
	printf("  Writing entities...\n");
	stream_write_u16_le(s, 5);         /* Number of entities */

	/* Entity 1: Player spawn */
	stream_write_u8(s, 1);             /* Type: Player */
	stream_write_i32_le(s, 100);       /* X position */
	stream_write_i32_le(s, 200);       /* Y position */
	stream_write_float_le(s, 0.0f);    /* Rotation */

	/* Entity 2-3: Enemies */
	for (int i = 0; i < 2; i++) {
		stream_write_u8(s, 2);         /* Type: Enemy */
		stream_write_i32_le(s, 500 + i * 100);
		stream_write_i32_le(s, 300);
		stream_write_float_le(s, 1.57f);
	}

	/* Entity 4-5: Powerups */
	for (int i = 0; i < 2; i++) {
		stream_write_u8(s, 3);         /* Type: Powerup */
		stream_write_i32_le(s, 700 + i * 50);
		stream_write_i32_le(s, 150);
		stream_write_float_le(s, 0.0f);
	}

	/* Write terrain data */
	printf("  Writing terrain...\n");
	stream_write_u32_le(s, 100);       /* Number of tiles */

	for (uint32_t i = 0; i < 100; i++) {
		stream_write_u8(s, i % 4);     /* Tile type */
		stream_write_u8(s, (i * 7) % 3); /* Variation */
	}

	off64_t size = stream_tell(s);
	printf("  Total bytes written: %lld\n", (long long)size);

	stream_close(s);
	return 0;
}

/* Example: Write network packet format (big-endian) */
static int write_network_packet(const char *filename)
{
	printf("\nWriting network packet to: %s\n", filename);

	struct file_stream fs;
	if (file_stream_open(&fs, filename, O_WRONLY | O_CREAT | O_TRUNC,
			     0644) < 0) {
		return -1;
	}

	struct stream *s = &fs.base;

	/* Packet header (big-endian for network byte order) */
	printf("  Writing packet header (big-endian)...\n");
	stream_write_u16_be(s, 0x8001);    /* Protocol ID */
	stream_write_u16_be(s, 42);        /* Packet type */
	stream_write_u32_be(s, 12345);     /* Sequence number */
	stream_write_u32_be(s, 1704067200); /* Timestamp */

	/* Packet payload */
	printf("  Writing packet payload...\n");
	stream_write_string(s, "player123");
	stream_write_u8(s, 100);           /* Health */
	stream_write_i16_be(s, 1024);      /* X position */
	stream_write_i16_be(s, 768);       /* Y position */
	stream_write_u16_be(s, 45);        /* Rotation */

	/* Checksum */
	stream_write_u32_be(s, 0xDEADBEEF);

	off64_t size = stream_tell(s);
	printf("  Total bytes written: %lld\n", (long long)size);

	stream_close(s);
	return 0;
}

/* Example: Write compressed binary data */
static int write_compressed_data(const char *filename)
{
	printf("\nWriting compressed binary data to: %s\n", filename);

	/* Open file */
	struct file_stream fs;
	if (file_stream_open(&fs, filename, O_WRONLY | O_CREAT | O_TRUNC,
			     0644) < 0) {
		return -1;
	}

	/* Add gzip compression */
	struct compression_stream cs;
	if (compression_stream_init(&cs, &fs.base, COMPRESS_GZIP,
				    O_WRONLY, 1) < 0) {
		stream_close(&fs.base);
		return -1;
	}

	struct stream *s = &cs.base;

	printf("  Writing data (will be gzip-compressed)...\n");

	/* Write header */
	stream_write(s, "DATA", 4);
	stream_write_u32_le(s, 1000);      /* Record count */

	/* Write records (each record is 9 bytes: 4 + 4 + 1) */
	for (int i = 0; i < 1000; i++) {
		stream_write_u32_le(s, i);     /* ID */
		stream_write_float_le(s, i * 1.5f); /* Value */
		stream_write_u8(s, i % 10);    /* Category */
	}

	/* Calculate uncompressed size: 4 (magic) + 4 (count) + 1000 * 9 (records) */
	size_t uncompressed = 4 + 4 + 1000 * 9;
	printf("  Uncompressed bytes: %zu\n", uncompressed);

	stream_close(s);

	/* Check compressed size */
	if (file_stream_open(&fs, filename, O_RDONLY, 0) == 0) {
		off64_t compressed_size = stream_size(&fs.base);
		printf("  Compressed size: %lld bytes (%.1f%% ratio)\n",
		       (long long)compressed_size,
		       (compressed_size * 100.0) / uncompressed);
		stream_close(&fs.base);
	}

	return 0;
}

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	printf("Binary File Writer - Stream API Demo\n");
	printf("====================================\n\n");
	printf("Demonstrates stream binary I/O functions:\n");
	printf("  - stream_write_u8/i8/u16/i16/u32/i32/u64/float/double\n");
	printf("  - Little-endian (_le) and big-endian (_be) variants\n");
	printf("  - stream_write_string() for length-prefixed strings\n");
	printf("  - Works with all stream types (file, memory, compressed)\n\n");

	/* Example 1: Game level data (little-endian) */
	write_game_level("level1.dat");

	/* Example 2: Network packet (big-endian) */
	write_network_packet("packet.bin");

	/* Example 3: Compressed binary data */
	write_compressed_data("data.bin.gz");

	printf("\nAll examples completed successfully!\n");
	printf("\nFiles created:\n");
	printf("  level1.dat     - Game level format (little-endian)\n");
	printf("  packet.bin     - Network packet (big-endian)\n");
	printf("  data.bin.gz    - Compressed binary data (gzip)\n");

	return 0;
}
