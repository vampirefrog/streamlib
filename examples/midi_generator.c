/**
 * @file midi_generator.c
 * @brief MIDI file generator using in-memory streams
 *
 * Demonstrates generating MIDI tracks in memory using mem_stream,
 * then writing to a Standard MIDI File (SMF) using file_stream.
 */

#include <stream.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* MIDI event structures */
struct midi_event {
	uint32_t delta_time;
	uint8_t status;
	uint8_t data1;
	uint8_t data2;
};

/* Write variable-length quantity (VLQ) used in MIDI */
static int write_vlq(struct stream *s, uint32_t value)
{
	uint8_t buf[4];
	int len = 0;

	/* Build VLQ bytes in reverse */
	buf[0] = value & 0x7F;
	value >>= 7;
	while (value > 0) {
		buf[++len] = (value & 0x7F) | 0x80;
		value >>= 7;
	}

	/* Write in correct order */
	for (int i = len; i >= 0; i--) {
		if (stream_write(s, &buf[i], 1) != 1)
			return -1;
	}

	return 0;
}

/* Note: Using stream_write_u32_be() and stream_write_u16_be() from stream API */

/* Write MIDI event to track stream */
static int write_event(struct stream *s, const struct midi_event *ev)
{
	/* Write delta time */
	if (write_vlq(s, ev->delta_time) < 0)
		return -1;

	/* Write status byte */
	if (stream_write(s, &ev->status, 1) != 1)
		return -1;

	/* Write data bytes (most MIDI events have 2 data bytes) */
	uint8_t status_type = ev->status & 0xF0;
	if (status_type == 0xC0 || status_type == 0xD0) {
		/* Program change and channel pressure have 1 data byte */
		if (stream_write(s, &ev->data1, 1) != 1)
			return -1;
	} else {
		/* Most events have 2 data bytes */
		if (stream_write(s, &ev->data1, 1) != 1)
			return -1;
		if (stream_write(s, &ev->data2, 1) != 1)
			return -1;
	}

	return 0;
}

/* Generate a simple melody track */
static struct stream *generate_melody_track(void)
{
	/* Create in-memory stream for track */
	struct mem_stream *ms = mem_stream_new(4096);
	if (!ms)
		return NULL;

	struct stream *s = &ms->base;

	/* Simple C major scale: C D E F G A B C */
	uint8_t notes[] = { 60, 62, 64, 65, 67, 69, 71, 72 };
	int num_notes = sizeof(notes) / sizeof(notes[0]);

	/* Write track events */
	for (int i = 0; i < num_notes; i++) {
		/* Note On */
		struct midi_event ev_on = {
			.delta_time = (i == 0) ? 0 : 480,  /* Quarter note spacing */
			.status = 0x90,  /* Note On, channel 0 */
			.data1 = notes[i],  /* Note number */
			.data2 = 64  /* Velocity */
		};
		write_event(s, &ev_on);

		/* Note Off after quarter note */
		struct midi_event ev_off = {
			.delta_time = 480,
			.status = 0x80,  /* Note Off, channel 0 */
			.data1 = notes[i],
			.data2 = 0
		};
		write_event(s, &ev_off);
	}

	/* End of track */
	uint8_t eot[] = { 0x00, 0xFF, 0x2F, 0x00 };
	stream_write(s, eot, sizeof(eot));

	return s;
}

/* Generate a simple drum track */
static struct stream *generate_drum_track(void)
{
	struct mem_stream *ms = mem_stream_new(2048);
	if (!ms)
		return NULL;

	struct stream *s = &ms->base;

	/* Simple 4/4 beat pattern (kick, snare, kick, snare) */
	uint8_t drums[] = { 36, 38, 36, 38 };  /* Bass drum, snare */

	for (int i = 0; i < 8; i++) {  /* 8 measures */
		for (int j = 0; j < 4; j++) {
			struct midi_event ev = {
				.delta_time = (i == 0 && j == 0) ? 0 : 480,
				.status = 0x99,  /* Note On, channel 9 (drums) */
				.data1 = drums[j % 4],
				.data2 = 96
			};
			write_event(s, &ev);

			/* Note off */
			ev.delta_time = 120;
			ev.status = 0x89;
			ev.data2 = 0;
			write_event(s, &ev);
		}
	}

	/* End of track */
	uint8_t eot[] = { 0x00, 0xFF, 0x2F, 0x00 };
	stream_write(s, eot, sizeof(eot));

	return s;
}

/* Write MIDI file from memory tracks to disk */
static int write_midi_file(const char *filename, struct stream **tracks,
			   int num_tracks)
{
	struct file_stream fs;
	if (file_stream_open(&fs, filename, O_WRONLY | O_CREAT | O_TRUNC, 0644) < 0) {
		fprintf(stderr, "Failed to create %s\n", filename);
		return -1;
	}

	struct stream *s = &fs.base;

	/* Write MThd header */
	stream_write(s, "MThd", 4);
	stream_write_u32_be(s, 6);  /* Header length */
	stream_write_u16_be(s, 1);  /* Format 1 (multiple tracks) */
	stream_write_u16_be(s, num_tracks);  /* Number of tracks */
	stream_write_u16_be(s, 480);  /* Ticks per quarter note */

	/* Write each track */
	for (int i = 0; i < num_tracks; i++) {
		off64_t track_size = stream_size(tracks[i]);

		/* Write MTrk header */
		stream_write(s, "MTrk", 4);
		stream_write_u32_be(s, track_size);

		/* Copy track data from memory to file */
		stream_seek(tracks[i], 0, SEEK_SET);
		char buf[4096];
		ssize_t remaining = track_size;
		while (remaining > 0) {
			size_t to_read = (remaining < 4096) ? remaining : 4096;
			ssize_t nread = stream_read(tracks[i], buf, to_read);
			if (nread <= 0)
				break;
			stream_write(s, buf, nread);
			remaining -= nread;
		}
	}

	stream_close(s);
	return 0;
}

int main(int argc, char **argv)
{
	const char *output = "output.mid";
	if (argc > 1)
		output = argv[1];

	printf("MIDI File Generator\n");
	printf("===================\n\n");

	/* Generate tracks in memory */
	printf("Generating melody track in memory...\n");
	struct stream *melody = generate_melody_track();
	if (!melody) {
		fprintf(stderr, "Failed to generate melody track\n");
		return 1;
	}
	printf("  Melody track: %lld bytes\n", (long long)stream_size(melody));

	printf("Generating drum track in memory...\n");
	struct stream *drums = generate_drum_track();
	if (!drums) {
		fprintf(stderr, "Failed to generate drum track\n");
		mem_stream_destroy((struct mem_stream *)melody);
		return 1;
	}
	printf("  Drum track: %lld bytes\n", (long long)stream_size(drums));

	/* Write all tracks to MIDI file */
	printf("\nWriting MIDI file: %s\n", output);
	struct stream *tracks[] = { melody, drums };
	if (write_midi_file(output, tracks, 2) < 0) {
		mem_stream_destroy((struct mem_stream *)melody);
		mem_stream_destroy((struct mem_stream *)drums);
		return 1;
	}

	/* Get final file size */
	struct file_stream fs;
	if (file_stream_open(&fs, output, O_RDONLY, 0) == 0) {
		printf("  Output file size: %lld bytes\n",
		       (long long)stream_size(&fs.base));
		stream_close(&fs.base);
	}

	/* Cleanup */
	mem_stream_destroy((struct mem_stream *)melody);
	mem_stream_destroy((struct mem_stream *)drums);

	printf("\nSuccess! Play with: timidity %s\n", output);
	return 0;
}
