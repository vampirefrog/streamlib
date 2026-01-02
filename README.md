![CI](https://github.com/vampirefrog/streamlib/actions/workflows/ci.yml/badge.svg)
![Release](https://github.com/vampirefrog/streamlib/actions/workflows/release.yml/badge.svg)

# StreamLib - Cross-Platform Stream I/O Library

A unified stream-based I/O library with optional support for compression and archives.

## Features

- **Cross-platform**: Windows, Linux, macOS, BSD
- **Large file support**: 64-bit offsets throughout
- **Memory-mapped I/O**: Native mmap with fallback emulation
- **Optional compression**: gzip, bzip2, xz, zstd with automatic format detection
- **Optional archive support**: ZIP, TAR, and more via libarchive
- **Path walker**: Recursive directory traversal with archive expansion
- **Transparent decompression**: Automatically decompress files in archives
- **Unified API**: Consistent interface across all stream types

## Building

### Minimal Build (Core Only)
```bash
mkdir build && cd build
cmake -DENABLE_ZLIB=OFF \
      -DENABLE_BZIP2=OFF \
      -DENABLE_LZMA=OFF \
      -DENABLE_ZSTD=OFF \
      -DENABLE_LIBARCHIVE=OFF ..
make
```

### Full-Featured Build
```bash
mkdir build && cd build
cmake ..
make
```

### Build Options

- `ENABLE_ZLIB` - Enable gzip/zlib support (default: ON)
- `ENABLE_BZIP2` - Enable bzip2 support (default: ON)
- `ENABLE_LZMA` - Enable xz/lzma support (default: ON)
- `ENABLE_ZSTD` - Enable zstd support (default: ON)
- `ENABLE_LIBARCHIVE` - Enable archive support (default: ON)
- `BUILD_TESTS` - Build test suite (default: ON)
- `BUILD_EXAMPLES` - Build examples (default: ON)

## Quick Start

### Basic File I/O
```c
#include <stream.h>

// Read a file
struct file_stream fs;
file_stream_open(&fs, "data.txt", O_RDONLY, 0);
char buf[1024];
ssize_t n = stream_read(&fs.base, buf, sizeof(buf));
stream_close(&fs.base);
```

### Automatic Compression Detection
```c
// Automatically detect and decompress any supported format
// Works with: .gz, .bz2, .xz, .zst, or ANY file with valid compression headers
#ifdef HAVE_COMPRESSION
struct file_stream fs;
struct compression_stream cs;

file_stream_open(&fs, "file.vgz", O_RDONLY, 0);  // Non-standard extension
if (compression_stream_auto(&cs, &fs.base, 1) == 0) {
    // Automatically detected as gzip by magic bytes (1f 8b)
    stream_read(&cs.base, buf, sizeof(buf));
    stream_close(&cs.base);
}
#endif
```

### Path Walker with Transparent Decompression
```c
// Walk directory tree, expanding archives and decompressing files
static int callback(const struct walker_entry *entry, void *userdata) {
    printf("%s\n", entry->name);

    // entry->stream is already decompressed and ready to read!
    if (entry->stream && !entry->is_dir) {
        char buf[256];
        ssize_t n = stream_read(entry->stream, buf, sizeof(buf));
        // Process decompressed data...
    }
    return 0;
}

// Walk and read .vgz files inside ZIP archives (automatically decompressed)
walk_path("music.zip", callback, NULL,
          WALK_EXPAND_ARCHIVES | WALK_DECOMPRESS | WALK_RECURSE_DIRS);
```

### Compression Format Support
| Format | Extension | Magic Bytes | Status |
|--------|-----------|-------------|--------|
| gzip   | .gz       | `1f 8b`     | ✓ Supported |
| bzip2  | .bz2      | `42 5a 68` ("BZh") | ✓ Supported |
| xz     | .xz       | `fd 37 7a 58 5a 00` | ✓ Supported |
| zstd   | .zst      | `28 b5 2f fd` | ✓ Supported |

**Note**: All formats are detected by magic bytes, so files with non-standard extensions (e.g., `.vgz`, `.vbz2`) are automatically recognized.

## API Reference

### Core Stream Interface

All stream types share a common base interface:

```c
struct stream; // Opaque struct which works as the base class for all other stream types

// Common operations (work on all stream types)
ssize_t stream_read(struct stream *s, void *buf, size_t count);
ssize_t stream_write(struct stream *s, const void *buf, size_t count);
off64_t stream_seek(struct stream *s, off64_t offset, int whence);
off64_t stream_tell(struct stream *s);
off64_t stream_size(struct stream *s);
void   *stream_mmap(struct stream *s, off64_t start, size_t length, int prot);
int     stream_munmap(struct stream *s, void *addr, size_t length);
int     stream_flush(struct stream *s);
int     stream_close(struct stream *s);
```

### File Streams

File-backed streams with native OS I/O:

```c
struct file_stream {
    struct stream base;
    // ...
};

int file_stream_open(struct file_stream *stream, const char *path,
                     int flags, mode_t mode);
int file_stream_from_fd(struct file_stream *stream, int fd, int flags);
```

### Memory Streams

In-memory buffers with stream interface:

```c
struct mem_stream {
    struct stream base;
    // ...
};

// Dynamic allocation (grows as needed)
int mem_stream_init_dynamic(struct mem_stream *stream, size_t initial_capacity);

// Use existing buffer
int mem_stream_init_buffer(struct mem_stream *stream, void *buf,
                           size_t size, int writable);

// Heap-allocated with ownership
struct mem_stream *mem_stream_new(size_t initial_capacity);
void mem_stream_free(struct mem_stream *stream);
```

### Compression Streams

Transparent compression/decompression (requires compression libraries):

```c
struct compression_stream {
    struct stream base;
    // ...
};

// Specific format
int compression_stream_init(struct compression_stream *stream,
                            struct stream *underlying,
                            enum compression_type type,
                            int flags, int owns_underlying);

// Auto-detect format by magic bytes
int compression_stream_auto(struct compression_stream *stream,
                            struct stream *underlying,
                            int owns_underlying);

// Convenience: auto-decompress or pass-through
struct stream *stream_auto_decompress(struct stream *source,
                                      struct compression_stream *cs_storage,
                                      int owns_source);
```

**Supported formats**: gzip (`.gz`), bzip2 (`.bz2`), xz (`.xz`), zstd (`.zst`)

### Archive Streams

Read archive entries (requires libarchive):

```c
struct archive_stream {
    struct stream base;
    // ...
};

int archive_stream_open(struct archive_stream *stream,
                        struct stream *underlying,
                        int owns_underlying);

void archive_stream_close(struct archive_stream *stream);

// Walk archive entries
typedef int (*archive_walker_fn)(const struct archive_entry_info *entry,
                                 void *userdata);

int archive_stream_walk(struct archive_stream *stream,
                       archive_walker_fn callback,
                       void *userdata);

struct archive_entry_info {
    const char *pathname;
    off64_t size;
    mode_t mode;
    time_t mtime;
    int is_dir;
};
```

**Supported formats**: ZIP, TAR, 7z, RAR, and more (via libarchive)

### Path Walker

Recursive directory/archive traversal:

```c
struct walker_entry {
    const char *path;           // Full path
    const char *name;           // Base name
    off64_t size;
    mode_t mode;
    time_t mtime;
    int is_dir;
    int is_archive_entry;       // From archive vs filesystem
    int depth;
    struct stream *stream;      // Open stream (read-ready)
    void *internal_data;
};

typedef int (*walker_fn)(const struct walker_entry *entry, void *userdata);

int walk_path(const char *path, walker_fn callback, void *userdata,
              unsigned int flags);
```

**Flags**:
- `WALK_RECURSE_DIRS` - Recurse into subdirectories
- `WALK_EXPAND_ARCHIVES` - Walk inside archive files
- `WALK_DECOMPRESS` - Auto-decompress compressed files
- `WALK_FILTER_FILES` - Only invoke callback for files
- `WALK_FILTER_DIRS` - Only invoke callback for directories
- `WALK_FOLLOW_SYMLINKS` - Follow symbolic links

### Feature Detection

```c
// Check if feature is available at runtime
int stream_has_feature(enum stream_feature feature);

// Features
enum stream_feature {
    STREAM_FEAT_LIBARCHIVE,
    STREAM_FEAT_ZLIB,
    STREAM_FEAT_BZIP2,
    STREAM_FEAT_LZMA,
    STREAM_FEAT_ZSTD
};

// Version info
const char *stream_get_version(void);
const char *stream_get_features_string(void);
```

### Compile-Time Feature Macros

```c
#ifdef HAVE_COMPRESSION    // Any compression library available
#ifdef HAVE_ZLIB           // gzip support
#ifdef HAVE_BZIP2          // bzip2 support
#ifdef HAVE_LZMA           // xz/lzma support
#ifdef HAVE_ZSTD           // zstd support
#ifdef HAVE_LIBARCHIVE     // Archive support
```

## Examples

The `examples/` directory contains practical demonstrations:

### walk_tree - Directory and Archive Walker
```bash
# Walk a directory recursively
./walk_tree /path/to/dir --recurse

# Expand and list archive contents
./walk_tree archive.zip --expand-archives

# Decompress and read .vgz files inside a ZIP archive
./walk_tree music.zip --expand-archives --decompress --show-content

# Walk compressed tar archives (.tar.gz, .tar.bz2, .tar.xz, .tar.zst)
./walk_tree backup.tar.zst --expand-archives --decompress

# Filter to show only files
./walk_tree /path --recurse --files-only
```

### midi_generator - In-Memory MIDI File Generation
Demonstrates generating MIDI tracks in memory using `mem_stream`, then writing them to a Standard MIDI File (SMF) using `file_stream`.

```bash
# Generate a MIDI file with melody and drum tracks
./midi_generator output.mid

# Play the generated file
timidity output.mid
```

**Key features demonstrated:**
- In-memory track generation with `mem_stream_create()`
- Writing binary data with proper endianness
- Combining multiple in-memory streams into a single output file
- Perfect for procedural music generation or MIDI processing tools

### vgz_analyzer - VGM/VGZ File Analysis
Recursively analyzes VGM (Video Game Music) files, including gzip-compressed .vgz files inside ZIP archives. Demonstrates the power of combining `walk_path()` with `WALK_EXPAND_ARCHIVES` and `WALK_DECOMPRESS`.

```bash
# Analyze VGM files in a directory
./vgz_analyzer /path/to/vgm/collection

# Works with nested structures: directories → ZIP files → .vgz files
./vgz_analyzer music.zip
```

**Key features demonstrated:**
- Recursive directory and archive traversal
- Automatic decompression of .vgz (gzipped VGM) files
- Binary format parsing from decompressed streams
- Statistics aggregation across multiple files

### binary_writer - Binary File Writing with Endianness Control
Shows how to write binary files one byte, short, or int32 at a time with specific endianness (big-endian or little-endian).

```bash
./binary_writer
# Creates: level1.dat (little-endian game data)
#          packet.bin (big-endian network packet)
#          data.bin.gz (compressed binary data)
```

**Key features demonstrated:**
- Writing bytes, shorts, and int32s with endianness control
- Creating custom binary file formats
- Writing to compressed streams (transparent gzip compression)
- Helper functions for common binary I/O patterns

### large_file_mmap - Memory-Mapped File Processing
```bash
# Search for pattern in large file
./large_file_mmap --search "pattern" largefile.bin

# Analyze byte statistics
./large_file_mmap --stats largefile.bin

# Calculate checksum
./large_file_mmap --checksum largefile.bin

# Process compressed file
./large_file_mmap --compressed --stats file.gz
```

## Use Cases

StreamLib is designed for real-world applications that need flexible stream I/O:

- **MIDI file generation**: Generate MIDI tracks in memory using `mem_stream`, then write to Standard MIDI Files (SMF). Perfect for procedural music generation, MIDI processing tools, or game audio engines. See `examples/midi_generator.c`.

- **VGZ file analysis**: Recursively process directories containing ZIP archives, which contain gzipped VGM files (.vgz). The library handles the entire chain automatically: filesystem → archive → decompression → your code. See `examples/vgz_analyzer.c`.

- **Binary file writing**: Write custom binary formats one byte, short, or int32 at a time with specific endianness. Supports writing to regular files, compressed streams, or in-memory buffers with a unified API. See `examples/binary_writer.c`.

- **Game music extraction**: Read .vgz (gzip-compressed VGM) files from ZIP archives with transparent decompression

- **Archive processing**: Extract and process files from nested archives with a single API call

- **Backup systems**: Read compressed tar archives with any compression format (.tar.gz, .tar.bz2, .tar.xz, .tar.zst)

- **Data pipelines**: Stream-based processing of compressed data without temporary files

- **Cross-platform tools**: Unified API across Windows, Linux, macOS, BSD

## Testing

Run the test suite:
```bash
cd build
./tests/test_compression  # Test all compression formats
./tests/test_walker       # Test directory/archive walking
```

All tests include verification of:
- 4 compression formats (gzip, bzip2, xz, zstd)
- Magic byte detection for non-standard extensions
- Transparent decompression in archives
- Round-trip compression/decompression

## License

Licensed under the GNU General Public License v3.0 - See LICENSE file for details.
