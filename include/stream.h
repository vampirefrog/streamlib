/* streamio.h - v1.0 - Cross-platform stream I/O library
 *
 * QUICK START:
 *
 *   Basic file reading:
 *      struct file_stream fs;
 *      file_stream_open(&fs, "data.txt", O_RDONLY, 0);
 *      stream_read(&fs.base, buf, sizeof(buf));
 *      stream_close(&fs.base);
 *
 *   Compressed file (auto-detect format by magic bytes):
 *      struct compression_stream cs;
 *      file_stream_open(&fs, "file.vgz", O_RDONLY, 0);  // Any extension works!
 *      compression_stream_auto(&cs, &fs.base, 1);        // Detects gzip/bzip2/xz/zstd
 *      stream_read(&cs.base, buf, sizeof(buf));
 *      stream_close(&cs.base);
 *
 *   Memory-map compressed file (emulated mmap):
 *      struct compression_stream cs;
 *      file_stream_open(&fs, "file.gz", O_RDONLY, 0);
 *      compression_stream_auto(&cs, &fs.base, 1);
 *      void *data = stream_mmap(&cs.base, 0, 65536, PROT_READ);  // Decompressed!
 *      // ... use data ...
 *      stream_munmap(&cs.base, data, 65536);
 *      stream_close(&cs.base);
 *
 *   Walk directory/archive (with transparent decompression):
 *      int callback(const struct walker_entry *e, void *data) {
 *          if (e->stream)
 *              stream_read(e->stream, buf, 256);  // Already decompressed!
 *          return 0;
 *      }
 *      walk_path("music.zip", callback, NULL,
 *                WALK_EXPAND_ARCHIVES | WALK_DECOMPRESS);
 *
 * FEATURES:
 *   - Unified stream API (file, memory, compression, archive)
 *   - 4 compression formats: gzip, bzip2, xz, zstd
 *   - Magic byte detection (files recognized by content, not extension)
 *   - Transparent decompression in archives
 *   - Path walker with directory/archive recursion
 *   - Large file support (64-bit offsets)
 *   - Memory-mapped I/O
 *
 * LICENSE: MIT
 */

#ifndef STREAM_H
#define STREAM_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/types.h>
#include <fcntl.h>

#ifdef _WIN32
#include <windows.h>
#include <sys/stat.h>
#include <io.h>
typedef __int64 off64_t;
typedef __int64 ssize_t;
typedef int mode_t;

/* POSIX function compatibility */
#define strdup _strdup

/* POSIX file flags for Windows */
#ifndef O_RDONLY
#define O_RDONLY _O_RDONLY
#endif
#ifndef O_WRONLY
#define O_WRONLY _O_WRONLY
#endif
#ifndef O_RDWR
#define O_RDWR _O_RDWR
#endif
#ifndef O_CREAT
#define O_CREAT _O_CREAT
#endif
#ifndef O_TRUNC
#define O_TRUNC _O_TRUNC
#endif
#ifndef O_ACCMODE
#define O_ACCMODE (_O_RDONLY | _O_WRONLY | _O_RDWR)
#endif

/* POSIX mmap flags for Windows */
#ifndef PROT_READ
#define PROT_READ 0x1
#endif
#ifndef PROT_WRITE
#define PROT_WRITE 0x2
#endif

/* POSIX stat macros for Windows */
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#endif
#ifndef S_ISLNK
#define S_ISLNK(m) 0  /* Windows doesn't have symlinks in the POSIX sense */
#endif

#else
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
typedef off_t off64_t;
#endif

#include "stream_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * VERSION & LIBRARY INFO
 * ============================================================================ */

#define STREAM_VERSION_MAJOR 1
#define STREAM_VERSION_MINOR 0
#define STREAM_VERSION_PATCH 0

/* Forward declarations */
struct stream;
struct stream_ops;

/* ============================================================================
 * CORE STREAM TYPES
 * ============================================================================ */

/* Stream capability flags - use stream_get_caps() to query */
enum stream_caps {
	STREAM_CAP_READ          = (1 << 0),  /* Can read */
	STREAM_CAP_WRITE         = (1 << 1),  /* Can write */
	STREAM_CAP_SEEK_SET      = (1 << 2),  /* Can seek to absolute position */
	STREAM_CAP_SEEK_CUR      = (1 << 3),  /* Can seek forward/backward from current */
	STREAM_CAP_SEEK_END      = (1 << 4),  /* Can seek relative to end */
	STREAM_CAP_MMAP          = (1 << 5),  /* Supports native mmap */
	STREAM_CAP_MMAP_EMULATED = (1 << 6),  /* Can emulate mmap via buffer */
	STREAM_CAP_TELL          = (1 << 7),  /* Can report current position */
	STREAM_CAP_SIZE          = (1 << 8),  /* Can report total size */
	STREAM_CAP_COMPRESSED    = (1 << 9),  /* Data is/will be compressed */
	STREAM_CAP_TRUNCATE      = (1 << 10), /* Can truncate */
	STREAM_CAP_FLUSH         = (1 << 11), /* Has meaningful flush operation */
};

/* Stream operations - virtual method table */
struct stream_ops {
	/* I/O operations */
	ssize_t (*read)(void *stream, void *buf, size_t count);
	ssize_t (*write)(void *stream, const void *buf, size_t count);

	/* Seeking - return new position or negative errno */
	off64_t (*seek)(void *stream, off64_t offset, int whence);
	off64_t (*tell)(void *stream);
	off64_t (*size)(void *stream);

	/* Memory mapping */
	void *(*mmap)(void *stream, off64_t start, size_t length, int prot);
	int (*munmap)(void *stream, void *addr, size_t length);

	/* Resource management */
	int (*flush)(void *stream);
	int (*close)(void *stream);

	/* Formatted output */
	int (*vprintf)(void *stream, const char *fmt, va_list ap);

	/* Capability query */
	unsigned int (*get_caps)(void *stream);
};

/* Base stream structure */
struct stream {
	const struct stream_ops *ops;
	unsigned int caps;      /* Cached capabilities (or computed) */
	int flags;              /* Open flags: O_RDONLY, O_WRONLY, O_RDWR, etc. */
	int error;              /* Last error (errno value) */
	off64_t pos;            /* Current position (if trackable) */
};

/* File stream structure */
struct file_stream {
	struct stream base;
#ifdef _WIN32
	HANDLE handle;
	HANDLE mapping_handle;
#else
	int fd;
#endif
	void *mmap_addr;
	size_t mmap_size;
	off64_t mmap_start;
	char *path;  /* For debugging */
};

/* Memory stream structure */
struct mem_stream {
	struct stream base;
	unsigned char *buf;
	size_t size;        /* Current size */
	size_t capacity;    /* Allocated capacity */
	off64_t pos;
	int owns_buffer;    /* Should free on close */
	int can_grow;       /* Can realloc if needed */
};

/* ============================================================================
 * COMPRESSION
 * ============================================================================ */

/* Compression formats - all detected automatically by magic bytes:
 *   COMPRESS_GZIP  - Magic: 1f 8b
 *   COMPRESS_BZIP2 - Magic: 42 5a 68 ("BZh")
 *   COMPRESS_XZ    - Magic: fd 37 7a 58 5a 00
 *   COMPRESS_ZSTD  - Magic: 28 b5 2f fd
 */
enum compression_type {
	COMPRESS_NONE = 0,
	COMPRESS_GZIP,      /* Requires ZLIB */
	COMPRESS_ZLIB,      /* Requires ZLIB */
	COMPRESS_BZIP2,     /* Requires BZIP2 */
	COMPRESS_XZ,        /* Requires LZMA */
	COMPRESS_LZMA,      /* Requires LZMA */
	COMPRESS_ZSTD,      /* Requires ZSTD */
};

/* Compression stream structure */
struct compression_stream {
	struct stream base;
	struct stream *underlying;  /* Stream being compressed/decompressed */
	int owns_underlying;        /* Should close underlying on close */

	/* Compression-specific state */
	void *codec_state;
	enum compression_type type;

	/* Buffering */
	unsigned char inbuf[16384];
	unsigned char outbuf[16384];
	size_t outbuf_used;
	size_t outbuf_pos;

	int at_eof;
	int is_writing;

	/* Emulated mmap support */
	void *emulated_mmap_addr;
	size_t emulated_mmap_size;
	off64_t emulated_mmap_start;
};

/* ============================================================================
 * ARCHIVES
 * ============================================================================ */

/* Archive entry metadata */
struct archive_entry_info {
	const char *pathname;       /* Full path within archive */
	const char *name;           /* Basename */
	off64_t size;               /* Uncompressed size */
	mode_t mode;                /* Unix permissions */
	time_t mtime;
	int is_dir;
	int is_compressed;          /* Entry itself is compressed */
};

/* Archive stream structure */
struct archive_stream {
	struct stream base;
	struct stream *underlying;   /* Stream containing archive data */
	int owns_underlying;

	void *archive;               /* libarchive handle */
	void *entry;                 /* Current entry */
	int entry_open;              /* Is an entry currently open for reading */
};

/* ============================================================================
 * PATH WALKER
 * ============================================================================ */

/* Walker entry - represents a file/directory/archive entry
 * The 'stream' field is automatically opened and can be read directly.
 * With WALK_DECOMPRESS, compressed files are already decompressed.
 */
struct walker_entry {
	const char *path;           /* Full path */
	const char *name;           /* Basename */
	off64_t size;
	mode_t mode;
	time_t mtime;
	int is_dir;
	int is_archive_entry;       /* False for filesystem, true for archive */
	int depth;                  /* Nesting depth */

	/* Stream for reading entry data (NULL for directories) */
	struct stream *stream;

	/* Internal data for opening the entry */
	void *internal_data;
};

/* Walk flags - combine with | operator */
enum walk_flags {
	/* Traversal options */
	WALK_RECURSE_DIRS     = (1 << 0),  /* Recurse into subdirectories */
	WALK_EXPAND_ARCHIVES  = (1 << 1),  /* Expand archive contents by magic bytes (ZIP, TAR, 7z, etc.) */
	WALK_DECOMPRESS       = (1 << 2),  /* Auto-decompress by magic bytes (not extension!) */
	WALK_FOLLOW_SYMLINKS  = (1 << 3),  /* Follow symbolic links */

	/* Filtering options */
	WALK_FILTER_FILES     = (1 << 8),  /* Only report regular files */
	WALK_FILTER_DIRS      = (1 << 9),  /* Only report directories */
};

/* ============================================================================
 * FEATURE DETECTION & VERSION INFO
 * ============================================================================ */

enum stream_features {
    STREAM_FEAT_ZLIB      = (1 << 0),
    STREAM_FEAT_BZIP2     = (1 << 1),
    STREAM_FEAT_LZMA      = (1 << 2),
    STREAM_FEAT_ZSTD      = (1 << 3),
    STREAM_FEAT_LIBARCHIVE = (1 << 4),
    STREAM_FEAT_MMAP      = (1 << 5),
};

unsigned int stream_get_features(void);                /* Returns bitfield */
int stream_has_feature(enum stream_features feature); /* 1 if available, 0 if not */
const char *stream_get_version(void);                  /* e.g., "1.0.0" */
const char *stream_get_features_string(void);          /* e.g., "zlib, bzip2, lzma, zstd" */

/* ============================================================================
 * STREAM API - Works with all stream types
 * ============================================================================ */

/* Base stream initialization - usually not called directly */
void stream_init(struct stream *stream, const struct stream_ops *ops,
		 int flags, unsigned int caps);

/* Generic stream operations - work with any stream type
 * These dispatch to the appropriate implementation via vtable */
ssize_t stream_read(struct stream *stream, void *buf, size_t count);
ssize_t stream_write(struct stream *stream, const void *buf, size_t count);
off64_t stream_seek(struct stream *stream, off64_t offset, int whence);  /* SEEK_SET/CUR/END */
off64_t stream_tell(struct stream *stream);  /* Current position */
off64_t stream_size(struct stream *stream);  /* Total size (if known) */

/* Memory mapping */
void *stream_mmap(struct stream *stream, off64_t start, size_t length, int prot);
int stream_munmap(struct stream *stream, void *addr, size_t length);

/* Utility functions */
int stream_flush(struct stream *stream);
int stream_close(struct stream *stream);
unsigned int stream_get_caps(struct stream *stream);

/* Capability testing helpers */
int stream_can_read(struct stream *stream);
int stream_can_write(struct stream *stream);
int stream_can_seek(struct stream *stream);
int stream_can_mmap(struct stream *stream);

/* Formatted output */
int stream_printf(struct stream *stream, const char *fmt, ...);
int stream_vprintf(struct stream *stream, const char *fmt, va_list ap);

/* Binary I/O helpers */
int stream_write_u8(struct stream *stream, uint8_t value);
int stream_write_i8(struct stream *stream, int8_t value);
int stream_write_u16_le(struct stream *stream, uint16_t value);
int stream_write_u16_be(struct stream *stream, uint16_t value);
int stream_write_i16_le(struct stream *stream, int16_t value);
int stream_write_i16_be(struct stream *stream, int16_t value);
int stream_write_u32_le(struct stream *stream, uint32_t value);
int stream_write_u32_be(struct stream *stream, uint32_t value);
int stream_write_i32_le(struct stream *stream, int32_t value);
int stream_write_i32_be(struct stream *stream, int32_t value);
int stream_write_u64_le(struct stream *stream, uint64_t value);
int stream_write_u64_be(struct stream *stream, uint64_t value);
int stream_write_float_le(struct stream *stream, float value);
int stream_write_float_be(struct stream *stream, float value);
int stream_write_double_le(struct stream *stream, double value);
int stream_write_double_be(struct stream *stream, double value);

/* Read functions */
int stream_read_u8(struct stream *stream, uint8_t *value);
int stream_read_i8(struct stream *stream, int8_t *value);
int stream_read_u16_le(struct stream *stream, uint16_t *value);
int stream_read_u16_be(struct stream *stream, uint16_t *value);
int stream_read_i16_le(struct stream *stream, int16_t *value);
int stream_read_i16_be(struct stream *stream, int16_t *value);
int stream_read_u32_le(struct stream *stream, uint32_t *value);
int stream_read_u32_be(struct stream *stream, uint32_t *value);
int stream_read_i32_le(struct stream *stream, int32_t *value);
int stream_read_i32_be(struct stream *stream, int32_t *value);
int stream_read_u64_le(struct stream *stream, uint64_t *value);
int stream_read_u64_be(struct stream *stream, uint64_t *value);
int stream_read_float_le(struct stream *stream, float *value);
int stream_read_float_be(struct stream *stream, float *value);
int stream_read_double_le(struct stream *stream, double *value);
int stream_read_double_be(struct stream *stream, double *value);

/* String I/O (writes length as uint16_t then string data) */
int stream_write_string(struct stream *stream, const char *str);
int stream_read_string(struct stream *stream, char *buf, size_t buf_size);

/* ============================================================================
 * FILE STREAMS
 * ============================================================================ */

/* Open a file stream
 *   path: file path
 *   flags: O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, etc.
 *   mode: permissions for O_CREAT (e.g., 0644)
 * Returns: 0 on success, negative errno on error */
int file_stream_open(struct file_stream *stream, const char *path, int flags,
		     mode_t mode);

/* Wrap an existing file descriptor */
int file_stream_from_fd(struct file_stream *stream, int fd, int flags);

/* ============================================================================
 * MEMORY STREAMS
 * ============================================================================ */

/* Initialize memory stream from existing buffer (does not copy data) */
int mem_stream_init(struct mem_stream *stream, void *buf, size_t size,
		    int writable);

/* Initialize growable memory stream (allocates buffer, for stack-allocated struct) */
int mem_stream_init_dynamic(struct mem_stream *stream, size_t initial_capacity);

/* Create new memory stream (allocates struct and buffer, use mem_stream_destroy to free) */
struct mem_stream *mem_stream_new(size_t initial_capacity);

/* Destroy memory stream created with mem_stream_new */
void mem_stream_destroy(struct mem_stream *stream);

/* Get pointer to stream's internal buffer */
const void *mem_stream_get_buffer(struct mem_stream *stream, size_t *size);

/* ============================================================================
 * COMPRESSION API
 * ============================================================================ */

#ifdef STREAM_HAVE_ZLIB

/* Initialize compression stream with specific format */
int compression_stream_init(struct compression_stream *stream,
			     struct stream *underlying,
			     enum compression_type type,
			     int flags,
			     int owns_underlying);

/* Convenience wrapper for gzip */
int gzip_stream_init(struct compression_stream *stream,
		     struct stream *underlying,
		     int flags,
		     int owns_underlying);

/* Check if compression format is available (1 = yes, 0 = no) */
int compression_is_available(enum compression_type type);

/* AUTO-DETECT compression format by reading magic bytes
 * This is the recommended way to handle compressed files!
 * Detects: gzip (1f 8b), bzip2 (42 5a 68), xz (fd 37...), zstd (28 b5...)
 *
 * Example:
 *   file_stream_open(&fs, "file.vgz", O_RDONLY, 0);  // Works with ANY extension
 *   compression_stream_auto(&cs, &fs.base, 1);        // Auto-detects gzip
 *   stream_read(&cs.base, buf, sizeof(buf));          // Read decompressed data
 *   stream_close(&cs.base);                           // Closes both streams
 *
 * Returns: 0 on success, -EINVAL if not compressed, negative errno on error */
int compression_stream_auto(struct compression_stream *stream,
			     struct stream *underlying,
			     int owns_underlying);

/* TRANSPARENT DECOMPRESSION - Simplest way to handle potentially compressed streams
 * Returns a stream that automatically decompresses if compression is detected.
 *
 * Example:
 *   struct compression_stream cs;
 *   struct stream *s = stream_auto_decompress(&file.base, &cs, 0);
 *   stream_read(s, buf, size);  // Automatically decompressed if needed!
 *   stream_close(s);            // Only close wrapper, not source (owns_source=0)
 *
 * Returns: Pointer to stream to use (source or &cs_storage->base), NULL on error */
struct stream *stream_auto_decompress(struct stream *source,
                                       struct compression_stream *cs_storage,
                                       int owns_source);
#endif /* STREAM_HAVE_ZLIB */

/* ============================================================================
 * ARCHIVE API (Low-level)
 * Most users should use walk_path() instead
 * ============================================================================ */

#ifdef STREAM_HAVE_LIBARCHIVE
/* Callback for iterating archive entries */
typedef int (*archive_walk_fn)(const struct archive_entry_info *entry,
			       void *userdata);

/* Open archive from stream (supports ZIP, TAR, etc.) */
int archive_stream_open(struct archive_stream *stream,
			struct stream *underlying,
			int owns_underlying);

/* Iterate through all entries in archive */
int archive_stream_walk(struct archive_stream *stream,
			archive_walk_fn callback,
			void *userdata);

/* Open specific entry for reading */
int archive_stream_open_entry(struct archive_stream *stream,
			       const struct archive_entry_info *entry);

/* Read from currently open entry */
ssize_t archive_stream_read_data(struct archive_stream *stream,
				  void *buf,
				  size_t count);

/* Close archive stream */
int archive_stream_close(struct archive_stream *stream);
#endif /* STREAM_HAVE_LIBARCHIVE */

/* ============================================================================
 * PATH WALKER API (Recommended for most use cases)
 * ============================================================================ */

/* Callback for walking entries
 * Return 0 to continue walking, non-zero to stop
 * The entry->stream is already opened and can be read directly! */
typedef int (*walker_fn)(const struct walker_entry *entry, void *userdata);

/* Walk filesystem/archive tree with optional decompression
 *
 * This is the most powerful function in the library. It can:
 *   - Walk directories recursively
 *   - Expand archive contents (ZIP, TAR, etc.)
 *   - Transparently decompress files (by magic bytes, not extension!)
 *   - Provide opened streams ready to read
 *
 * Example 1 - Walk directory:
 *   walk_path("/data", callback, NULL, WALK_RECURSE_DIRS);
 *
 * Example 2 - Read .vgz files from ZIP archive:
 *   walk_path("music.zip", callback, NULL,
 *             WALK_EXPAND_ARCHIVES | WALK_DECOMPRESS);
 *   // In callback: stream_read(entry->stream, ...) reads decompressed data!
 *
 * Example 3 - Extract compressed tar:
 *   walk_path("backup.tar.zst", callback, NULL,
 *             WALK_EXPAND_ARCHIVES | WALK_DECOMPRESS);
 *
 * Returns: 0 on success, negative errno on error */
int walk_path(const char *path, walker_fn callback, void *userdata,
	      unsigned int flags);

#ifdef __cplusplus
}
#endif

#endif /* STREAM_H */
