# Changelog

All notable changes to StreamLib will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2025-01-03

### Added

#### Core Features
- **Stream abstraction layer** - Unified interface for file, memory, compression, and archive streams
- **File streams** - POSIX and Windows support with mmap capabilities
- **Memory streams** - In-memory buffer operations with dynamic growth
- **Stream printf** - Formatted output to any stream type

#### Compression Support
- **Read support** for gzip, bzip2, xz/lzma, and zstd formats
- **Write support** for all compression formats
- **Automatic format detection** via magic byte analysis (not file extensions)
- **Transparent decompression** - Compressed files treated as regular streams
- **mmap emulation** for compressed streams

#### Archive Support
- **Read support** for ZIP, TAR, 7z, RAR, CPIO, ISO9660, SHAR and more (via libarchive)
- **Write support** for TAR (ustar/pax), ZIP, 7-Zip, CPIO, ISO9660, SHAR
- **Archive entry iteration** with callback-based walking
- **Nested compression** - Transparently decompress files within archives
- **Format detection** by magic bytes

#### Path Walker
- **Recursive directory traversal** (POSIX and Windows)
- **Archive expansion** - Walk through archive contents as if they were directories
- **Transparent decompression** - Automatically decompress files during traversal
- **Flexible filtering** - Filter by files, directories, or both
- **Depth control** and symlink handling

#### Build System
- **CMake-based** build system
- **Optional dependencies** - Enable/disable features at compile time
- **Cross-platform** - Linux, macOS, Windows support
- **vcpkg integration** for Windows dependencies

#### Testing
- **Comprehensive test suite** - 44 tests covering all major functionality
- **Zero memory leaks** verified with Valgrind
- **CI/CD pipeline** - GitHub Actions with Linux, macOS, and Windows builds
- **Static analysis** with cppcheck
- **Code coverage** reporting

#### Examples
- `read_file` - Basic file reading
- `walk_tree` - Directory and archive walking with decompression
- `read_gzip` - Reading compressed files
- `write_compressed` - Writing compressed files in all formats
- `compress_text` - Comparing compression ratios
- `list_archive` - Listing archive contents
- `create_archive` - Creating archives in various formats
- `large_file_mmap` - Large file processing with mmap
- `test_mmap_emulation` - mmap emulation demo
- `midi_generator` - In-memory file generation
- `vgz_analyzer` - Recursive archive + decompression
- `binary_writer` - Binary file writing with endianness control

#### Documentation
- Comprehensive README with API examples
- Annotated headers with usage examples
- Magic byte detection reference table
- Build and integration guides

### Technical Details

**Supported Compression Formats:**
- gzip/zlib (.gz) - Requires zlib
- bzip2 (.bz2) - Requires libbz2
- xz/lzma (.xz) - Requires liblzma
- zstd (.zst) - Requires libzstd

**Supported Archive Formats:**
- TAR (USTAR, PAX) - Read and write
- ZIP - Read and write
- 7-Zip - Read and write
- CPIO - Read and write
- ISO9660 - Read and write
- SHAR - Read and write
- RAR - Read only

**Platform Support:**
- Linux x64 - Fully tested
- macOS x64 - Fully tested
- Windows x64 - Fully tested with MinGW and MSVC

**API Stability:**
- Pre-1.0 release - API may change in future minor versions
- Following semantic versioning

### Known Limitations
- Archive modification (update existing archives) not yet supported
- Large file tests (>4GB) not yet comprehensive
- Error handling coverage could be improved

### Dependencies
- zlib (optional, for gzip support)
- libbz2 (optional, for bzip2 support)
- liblzma (optional, for xz support)
- libzstd (optional, for zstd support)
- libarchive (optional, for archive support)

[0.1.0]: https://github.com/YOUR_USERNAME/streamiolib/releases/tag/v0.1.0
