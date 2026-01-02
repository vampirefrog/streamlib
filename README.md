![CI](https://github.com/vampirefrog/streamlib/actions/workflows/ci.yml/badge.svg)
![Release](https://github.com/vampirefrog/streamlib/actions/workflows/release.yml/badge.svg)

# StreamLib - Cross-Platform Stream I/O Library

A unified stream-based I/O library with optional support for compression and archives.

## Features

- **Cross-platform**: Windows, Linux, macOS, BSD
- **Large file support**: 64-bit offsets throughout
- **Memory-mapped I/O**: Native mmap with fallback emulation
- **Optional compression**: gzip, bzip2, xz, zstd with automatic format detection
- **Magic byte detection**: Compressed files detected by content, not extension
- **Optional archive support**: ZIP, TAR, and more via libarchive
- **Path walker**: Recursive directory traversal with archive expansion
- **Transparent decompression**: Automatically decompress files in archives
- **Unified API**: Consistent interface across all stream types
- **OOP in C**: Virtual methods using function pointers

## Building

### Minimal Build (Core Only)
```bash
mkdir build && cd build
cmake -DSTREAM_ENABLE_ZLIB=OFF \
      -DSTREAM_ENABLE_BZIP2=OFF \
      -DSTREAM_ENABLE_LZMA=OFF \
      -DSTREAM_ENABLE_ZSTD=OFF \
      -DSTREAM_ENABLE_LIBARCHIVE=OFF ..
make
```

### Full-Featured Build
```bash
mkdir build && cd build
cmake ..
make
```

### Build Options

- `STREAM_ENABLE_ZLIB` - Enable gzip/zlib support (default: ON)
- `STREAM_ENABLE_BZIP2` - Enable bzip2 support (default: ON)
- `STREAM_ENABLE_LZMA` - Enable xz/lzma support (default: ON)
- `STREAM_ENABLE_ZSTD` - Enable zstd support (default: ON)
- `STREAM_ENABLE_LIBARCHIVE` - Enable archive support (default: ON)
- `STREAM_BUILD_TESTS` - Build test suite (default: ON)
- `STREAM_BUILD_EXAMPLES` - Build examples (default: ON)

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
#ifdef STREAM_HAVE_ZLIB
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

## Documentation

See [docs/API.md](docs/API.md) for full API documentation.

## License

MIT License - See LICENSE file for details.
