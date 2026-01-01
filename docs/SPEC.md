# Stream I/O Library Specification v1.1

## 1. Overview

A cross-platform C library providing unified stream-based I/O with support for files, memory buffers, archives, and transparent compression. The library follows Linux kernel coding conventions with a pseudo-OOP interface using function pointers as virtual methods.

**All archive and compression features are optional** and can be disabled at build time or detected at runtime.

## 2. Design Principles

- **Polymorphic Interface**: Base `stream` type with virtual method table
- **Inheritance Pattern**: Child structures embed parent, initialize via parent init function
- **Linux Kernel Style**: First parameter is `this`, negative errno returns
- **Cross-Platform**: Support Windows, Linux, macOS, BSD
- **Large File Support**: 64-bit offsets throughout (off64_t)
- **Composability**: Streams can wrap other streams (compression, buffering)
- **Zero-Copy Where Possible**: mmap support with fallback emulation
- **Optional Features**: All compression and archive support is optional

## 3. Feature Detection

### 3.1 Compile-Time Feature Flags

```c
/* Define these to enable optional features at compile time */
/* #define STREAMIO_HAVE_ZLIB      1 */
/* #define STREAMIO_HAVE_BZIP2     1 */
/* #define STREAMIO_HAVE_LZMA      1 */
/* #define STREAMIO_HAVE_ZSTD      1 */
/* #define STREAMIO_HAVE_LIBARCHIVE 1 */

/* Auto-generated header from build system */
/* streamio_config.h */
#ifndef STREAMIO_CONFIG_H
#define STREAMIO_CONFIG_H

/* These are set by CMake/build system */
#cmakedefine STREAMIO_HAVE_ZLIB @STREAMIO_HAVE_ZLIB@
#cmakedefine STREAMIO_HAVE_BZIP2 @STREAMIO_HAVE_BZIP2@
#cmakedefine STREAMIO_HAVE_LZMA @STREAMIO_HAVE_LZMA@
#cmakedefine STREAMIO_HAVE_ZSTD @STREAMIO_HAVE_ZSTD@
#cmakedefine STREAMIO_HAVE_LIBARCHIVE @STREAMIO_HAVE_LIBARCHIVE@

#endif /* STREAMIO_CONFIG_H */
```

### 3.2 Runtime Feature Detection

```c
/* Feature flags for runtime detection */
enum streamio_features {
    STREAMIO_FEAT_ZLIB      = (1 << 0),
    STREAMIO_FEAT_BZIP2     = (1 << 1),
    STREAMIO_FEAT_LZMA      = (1 << 2),
    STREAMIO_FEAT_ZSTD      = (1 << 3),
    STREAMIO_FEAT_LIBARCHIVE = (1 << 4),
    STREAMIO_FEAT_MMAP      = (1 << 5),  /* OS supports mmap */
};

/* Query available features at runtime */
unsigned int streamio_get_features(void);

/* Check if specific feature is available */
int streamio_has_feature(enum streamio_features feature);

/* Get version string */
const char *streamio_get_version(void);

/* Get feature list as string (for debugging) */
const char *streamio_get_features_string(void);
```

### 3.3 Compression Type Availability

```c
/* Compression codec identifiers */
enum compression_type {
    COMPRESS_NONE = 0,
    COMPRESS_GZIP,      /* Requires ZLIB */
    COMPRESS_ZLIB,      /* Requires ZLIB */
    COMPRESS_BZIP2,     /* Requires BZIP2 */
    COMPRESS_XZ,        /* Requires LZMA */
    COMPRESS_LZMA,      /* Requires LZMA */
    COMPRESS_ZSTD,      /* Requires ZSTD */
};

/* Check if compression type is available */
int compression_is_available(enum compression_type type);

/* Get list of available compression types */
int compression_get_available(enum compression_type *types, size_t max_count);
```

## 4. Core Types

### 4.1 Base Stream Structure

```c
/* Stream capability flags - bitfield */
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
    ssize_t (*read)(void *this, void *buf, size_t count);
    ssize_t (*write)(void *this, const void *buf, size_t count);

    /* Seeking - return new position or negative errno */
    off64_t (*seek)(void *this, off64_t offset, int whence);
    off64_t (*tell)(void *this);
    off64_t (*size)(void *this);

    /* Memory mapping */
    void *(*mmap)(void *this, off64_t start, size_t length, int prot);
    int (*munmap)(void *this, void *addr, size_t length);

    /* Resource management */
    int (*flush)(void *this);
    int (*close)(void *this);

    /* Formatted output */
    int (*vprintf)(void *this, const char *fmt, va_list ap);

    /* Capability query */
    unsigned int (*get_caps)(void *this);
};

/* Base stream structure */
struct stream {
    const struct stream_ops *ops;
    unsigned int caps;      /* Cached capabilities (or computed) */
    int flags;              /* Open flags: O_RDONLY, O_WRONLY, O_RDWR, etc. */
    int error;              /* Last error (errno value) */
    off64_t pos;            /* Current position (if trackable) */
};
```

### 4.2 Large File Support

```c
/* Platform-independent 64-bit offset type */
#ifdef _WIN32
typedef __int64 off64_t;
#define FOPEN_LARGE fopen  /* fopen on Windows handles large files */
#define FSEEK_LARGE _fseeki64
#define FTELL_LARGE _ftelli64
#else
/* Ensure large file support */
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64
typedef off_t off64_t;
#define FOPEN_LARGE fopen
#define FSEEK_LARGE fseeko
#define FTELL_LARGE ftello
#endif

/* Maximum file size supported (effectively unlimited on 64-bit) */
#define STREAM_MAX_SIZE INT64_MAX
```

## 5. Public API

### 5.1 Stream Lifecycle

```c
/* Initialize base stream (called by subclasses) */
void stream_init(struct stream *this, const struct stream_ops *ops,
                 int flags, unsigned int caps);

/* Generic stream operations (dispatch to vtable) */
ssize_t stream_read(struct stream *this, void *buf, size_t count);
ssize_t stream_write(struct stream *this, const void *buf, size_t count);
off64_t stream_seek(struct stream *this, off64_t offset, int whence);
off64_t stream_tell(struct stream *this);
off64_t stream_size(struct stream *this);

/* Memory mapping - start offset allows mapping portions of large files */
void *stream_mmap(struct stream *this, off64_t start, size_t length, int prot);
int stream_munmap(struct stream *this, void *addr, size_t length);

/* Utility functions */
int stream_flush(struct stream *this);
int stream_close(struct stream *this);
unsigned int stream_get_caps(struct stream *this);

/* Capability testing helpers */
int stream_can_read(struct stream *this);
int stream_can_write(struct stream *this);
int stream_can_seek(struct stream *this);
int stream_can_seek_forward(struct stream *this);
int stream_can_seek_backward(struct stream *this);
int stream_can_seek_to_start(struct stream *this);
int stream_can_seek_to_end(struct stream *this);
int stream_can_mmap(struct stream *this);
int stream_can_mmap_native(struct stream *this);
int stream_is_compressed(struct stream *this);

/* Formatted output */
int stream_printf(struct stream *this, const char *fmt, ...);
int stream_vprintf(struct stream *this, const char *fmt, va_list ap);

/* Convenience functions */
ssize_t stream_read_all(struct stream *this, void **buf, size_t *size);
int stream_copy(struct stream *dst, struct stream *src);
```

### 5.2 File Stream

```c
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

/* Open file stream */
int file_stream_open(struct file_stream *this, const char *path, int flags,
                     mode_t mode);
int file_stream_from_fd(struct file_stream *this, int fd, int flags);

#ifdef _WIN32
int file_stream_from_handle(struct file_stream *this, HANDLE handle, int flags);
#endif
```

### 5.3 Memory Stream

```c
struct mem_stream {
    struct stream base;
    unsigned char *buf;
    size_t size;        /* Current size */
    size_t capacity;    /* Allocated capacity */
    off64_t pos;
    int owns_buffer;    /* Should free on close */
    int can_grow;       /* Can realloc if needed */
};

/* Initialize with existing buffer */
int mem_stream_init(struct mem_stream *this, void *buf, size_t size,
                    int writable);

/* Create growable memory stream */
int mem_stream_create(struct mem_stream *this, size_t initial_capacity);

/* Get buffer pointer (useful after writing) */
const void *mem_stream_get_buffer(struct mem_stream *this, size_t *size);
```

### 5.4 Compression Streams (Optional)

```c
#ifdef STREAMIO_HAVE_ZLIB
/* Generic compression stream */
struct compression_stream {
    struct stream base;
    struct stream *underlying;  /* Stream being compressed/decompressed */
    int owns_underlying;        /* Should close underlying on close */

    /* Compression-specific state (zlib, bzip2, etc.) */
    void *codec_state;
    const struct compression_ops *codec_ops;

    /* Buffering */
    unsigned char inbuf[16384];
    unsigned char outbuf[16384];
    size_t inbuf_used;
    size_t outbuf_used;
    size_t outbuf_pos;

    int at_eof;
};

/* Create compression stream wrapping another stream */
int compression_stream_init(struct compression_stream *this,
                             struct stream *underlying,
                             enum compression_type type,
                             int flags,
                             int owns_underlying);

/* Convenience wrappers - only available if respective library is present */
#ifdef STREAMIO_HAVE_ZLIB
int gzip_stream_init(struct compression_stream *this, struct stream *underlying,
                     int flags, int owns_underlying);
int zlib_stream_init(struct compression_stream *this, struct stream *underlying,
                     int flags, int owns_underlying);
#endif

#ifdef STREAMIO_HAVE_BZIP2
int bzip2_stream_init(struct compression_stream *this, struct stream *underlying,
                      int flags, int owns_underlying);
#endif

#ifdef STREAMIO_HAVE_LZMA
int xz_stream_init(struct compression_stream *this, struct stream *underlying,
                   int flags, int owns_underlying);
int lzma_stream_init(struct compression_stream *this, struct stream *underlying,
                     int flags, int owns_underlying);
#endif

#ifdef STREAMIO_HAVE_ZSTD
int zstd_stream_init(struct compression_stream *this, struct stream *underlying,
                     int flags, int owns_underlying);
#endif

/* Auto-detect compression from magic bytes and create appropriate stream */
/* Returns -ENOSYS if no compression libraries available */
/* Returns -ENOTSUP if compression type not supported */
int compression_stream_auto(struct compression_stream *this,
                             struct stream *underlying,
                             int owns_underlying);

#endif /* STREAMIO_HAVE_ZLIB || STREAMIO_HAVE_BZIP2 || ... */
```

### 5.5 Archive Support (Optional)

```c
#ifdef STREAMIO_HAVE_LIBARCHIVE

/* Archive entry metadata */
struct archive_entry_info {
    const char *pathname;       /* Full path within archive */
    const char *name;           /* Basename */
    off64_t size;               /* Uncompressed size */
    off64_t compressed_size;    /* Compressed size (if applicable) */
    mode_t mode;                /* Unix permissions */
    time_t mtime;
    uid_t uid;
    gid_t gid;
    int is_dir;
    int is_compressed;          /* Entry itself is compressed */

    /* Opaque handle for opening this entry */
    void *archive_handle;
    void *entry_handle;
};

/* Callback for walking archive entries */
typedef int (*archive_walk_fn)(const struct archive_entry_info *entry,
                                void *userdata);

/* Archive stream - represents an opened archive */
struct archive_stream {
    struct stream base;
    struct stream *underlying;   /* Stream containing archive data */
    int owns_underlying;

    struct archive *ar;          /* libarchive handle */
    struct archive_entry *current_entry;

    /* For nested archives/compression */
    struct compression_stream *decompressor;
};

/* Open archive from stream */
int archive_stream_open(struct archive_stream *this, struct stream *underlying,
                        int owns_underlying);

/* Walk all entries in archive */
int archive_stream_walk(struct archive_stream *this, archive_walk_fn callback,
                        void *userdata);

/* Open specific entry for reading - returns new stream */
int archive_stream_open_entry(struct archive_stream *this,
                               const struct archive_entry_info *entry,
                               struct stream **out);

/* Get entry by path */
int archive_stream_find_entry(struct archive_stream *this, const char *path,
                               struct archive_entry_info **out);

#endif /* STREAMIO_HAVE_LIBARCHIVE */
```

### 5.6 Unified Path Walking

```c
/* Walker entry - can be filesystem or archive */
struct walker_entry {
    const char *path;           /* Full virtual path */
    const char *name;           /* Basename */
    off64_t size;
    mode_t mode;
    time_t mtime;
    int is_dir;
    int is_archive_entry;       /* False for filesystem, true for archive */
    int depth;                  /* Nesting depth */

    /* For opening the entry as a stream */
    union {
        char *fs_path;          /* Filesystem path */
#ifdef STREAMIO_HAVE_LIBARCHIVE
        struct {
            struct archive_stream *archive;
            struct archive_entry_info *entry;
        } ar;
#endif
    } source;
};

/* Walker callback - return 0 to continue, non-zero to stop */
typedef int (*walker_fn)(const struct walker_entry *entry, void *userdata);

/* Walk flags */
enum walk_flags {
    /* Traversal options */
    WALK_RECURSE_DIRS     = (1 << 0),  /* Recurse into subdirectories */
    WALK_EXPAND_ARCHIVES  = (1 << 1),  /* Expand archive contents (requires libarchive) */
    WALK_DECOMPRESS       = (1 << 2),  /* Auto-decompress compressed files (requires compression libs) */
    WALK_FOLLOW_SYMLINKS  = (1 << 3),  /* Follow symbolic links */
    WALK_DEPTH_FIRST      = (1 << 4),  /* Depth-first vs breadth-first */

    /* Filtering options - only invoke callback for entries matching these */
    WALK_FILTER_READABLE  = (1 << 8),  /* Only entries that can be read */
    WALK_FILTER_WRITABLE  = (1 << 9),  /* Only entries that can be written */
    WALK_FILTER_SEEKABLE  = (1 << 10), /* Only entries that support seeking */
    WALK_FILTER_MMAPABLE  = (1 << 11), /* Only entries that can be mmap'd */
    WALK_FILTER_FILES     = (1 << 12), /* Only regular files */
    WALK_FILTER_DIRS      = (1 << 13), /* Only directories */

    /* Stream requirements - entries must support these capabilities */
    WALK_REQUIRE_SEEK_FWD = (1 << 16), /* Entry stream must support forward seeking */
    WALK_REQUIRE_SEEK_BWD = (1 << 17), /* Entry stream must support backward seeking */
    WALK_REQUIRE_SEEK_ABS = (1 << 18), /* Entry stream must support absolute seeking */
    WALK_REQUIRE_MMAP     = (1 << 19), /* Entry stream must support mmap (native or emulated) */
    WALK_REQUIRE_MMAP_NATIVE = (1 << 20), /* Entry stream must support native mmap */
    WALK_REQUIRE_SIZE     = (1 << 21), /* Entry stream must be able to report size */
};

/* Walk filesystem path, optionally expanding archives */
/* Returns:
 *   0 on success (walked all entries)
 *   Positive value if callback requested stop
 *   -ENOSYS if requested feature not available (e.g., WALK_EXPAND_ARCHIVES without libarchive)
 *   -errno on error
 */
int walk_path(const char *path, walker_fn callback, void *userdata,
              unsigned int flags);

/* Open entry from walker as stream
 * The returned stream will meet the capability requirements specified in walk flags
 */
int walker_entry_open(const struct walker_entry *entry, struct stream **out);

/* Check if entry meets capability requirements */
int walker_entry_check_caps(const struct walker_entry *entry, unsigned int required_caps);
```

## 6. Implementation Details

### 6.1 Feature Detection Implementation

```c
/* streamio.c */
unsigned int streamio_get_features(void)
{
    unsigned int features = 0;

#ifdef STREAMIO_HAVE_ZLIB
    features |= STREAMIO_FEAT_ZLIB;
#endif

#ifdef STREAMIO_HAVE_BZIP2
    features |= STREAMIO_FEAT_BZIP2;
#endif

#ifdef STREAMIO_HAVE_LZMA
    features |= STREAMIO_FEAT_LZMA;
#endif

#ifdef STREAMIO_HAVE_ZSTD
    features |= STREAMIO_FEAT_ZSTD;
#endif

#ifdef STREAMIO_HAVE_LIBARCHIVE
    features |= STREAMIO_FEAT_LIBARCHIVE;
#endif

#if defined(_WIN32) || defined(__unix__) || defined(__APPLE__)
    features |= STREAMIO_FEAT_MMAP;
#endif

    return features;
}

int streamio_has_feature(enum streamio_features feature)
{
    return !!(streamio_get_features() & feature);
}

const char *streamio_get_features_string(void)
{
    static char buf[256];
    char *p = buf;
    int first = 1;

    *p = '\0';

#ifdef STREAMIO_HAVE_ZLIB
    p += sprintf(p, "%szlib", first ? "" : ", ");
    first = 0;
#endif

#ifdef STREAMIO_HAVE_BZIP2
    p += sprintf(p, "%sbzip2", first ? "" : ", ");
    first = 0;
#endif

#ifdef STREAMIO_HAVE_LZMA
    p += sprintf(p, "%slzma", first ? "" : ", ");
    first = 0;
#endif

#ifdef STREAMIO_HAVE_ZSTD
    p += sprintf(p, "%szstd", first ? "" : ", ");
    first = 0;
#endif

#ifdef STREAMIO_HAVE_LIBARCHIVE
    p += sprintf(p, "%slibarchive", first ? "" : ", ");
    first = 0;
#endif

    if (first)
        sprintf(buf, "none");

    return buf;
}

int compression_is_available(enum compression_type type)
{
    switch (type) {
    case COMPRESS_NONE:
        return 1;

#ifdef STREAMIO_HAVE_ZLIB
    case COMPRESS_GZIP:
    case COMPRESS_ZLIB:
        return 1;
#endif

#ifdef STREAMIO_HAVE_BZIP2
    case COMPRESS_BZIP2:
        return 1;
#endif

#ifdef STREAMIO_HAVE_LZMA
    case COMPRESS_XZ:
    case COMPRESS_LZMA:
        return 1;
#endif

#ifdef STREAMIO_HAVE_ZSTD
    case COMPRESS_ZSTD:
        return 1;
#endif

    default:
        return 0;
    }
}
```

### 6.2 Walker with Capability Filtering

```c
/* walker.c */

/* Internal: Check if a stream meets capability requirements */
static int check_stream_caps(struct stream *s, unsigned int flags)
{
    unsigned int caps = stream_get_caps(s);

    /* Check required capabilities */
    if (flags & WALK_REQUIRE_SEEK_FWD) {
        if (!(caps & STREAM_CAP_SEEK_CUR))
            return 0;
    }

    if (flags & WALK_REQUIRE_SEEK_BWD) {
        if (!(caps & STREAM_CAP_SEEK_CUR))
            return 0;
    }

    if (flags & WALK_REQUIRE_SEEK_ABS) {
        if (!(caps & STREAM_CAP_SEEK_SET))
            return 0;
    }

    if (flags & WALK_REQUIRE_MMAP) {
        if (!(caps & (STREAM_CAP_MMAP | STREAM_CAP_MMAP_EMULATED)))
            return 0;
    }

    if (flags & WALK_REQUIRE_MMAP_NATIVE) {
        if (!(caps & STREAM_CAP_MMAP))
            return 0;
    }

    if (flags & WALK_REQUIRE_SIZE) {
        if (!(caps & STREAM_CAP_SIZE))
            return 0;
    }

    return 1;
}

int walker_entry_check_caps(const struct walker_entry *entry,
                             unsigned int required_caps)
{
    struct stream *s;
    int ret;

    /* Try to open the entry */
    ret = walker_entry_open(entry, &s);
    if (ret < 0)
        return 0;

    /* Check capabilities */
    ret = check_stream_caps(s, required_caps);

    stream_close(s);
    return ret;
}

static int walk_directory_impl(const char *path, walker_fn callback,
                                void *userdata, unsigned int flags, int depth);

#ifdef STREAMIO_HAVE_LIBARCHIVE
static int walk_archive_impl(const char *path, struct stream *stream,
                              walker_fn callback, void *userdata,
                              unsigned int flags, int depth)
{
    struct archive_stream ar;
    int ret;

    ret = archive_stream_open(&ar, stream, 1);
    if (ret < 0)
        return ret;

    /* Walk archive entries */
    struct archive_entry *entry;
    struct archive *a = ar.ar;

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char *pathname = archive_entry_pathname(entry);
        struct walker_entry wentry = {
            .path = pathname,
            .name = /* basename(pathname) */,
            .size = archive_entry_size(entry),
            .mode = archive_entry_mode(entry),
            .mtime = archive_entry_mtime(entry),
            .is_dir = S_ISDIR(archive_entry_mode(entry)),
            .is_archive_entry = 1,
            .depth = depth,
        };

        /* Apply filters */
        if (flags & WALK_FILTER_FILES && wentry.is_dir)
            goto skip;
        if (flags & WALK_FILTER_DIRS && !wentry.is_dir)
            goto skip;

        /* Check capability requirements if specified */
        if (flags & (WALK_REQUIRE_SEEK_FWD | WALK_REQUIRE_SEEK_BWD |
                     WALK_REQUIRE_SEEK_ABS | WALK_REQUIRE_MMAP |
                     WALK_REQUIRE_MMAP_NATIVE | WALK_REQUIRE_SIZE)) {
            if (!walker_entry_check_caps(&wentry, flags))
                goto skip;
        }

        /* Invoke callback */
        ret = callback(&wentry, userdata);
        if (ret != 0) {
            archive_stream_close(&ar);
            return ret;
        }

skip:
        archive_read_data_skip(a);
    }

    stream_close(&ar.base);
    return 0;
}
#endif /* STREAMIO_HAVE_LIBARCHIVE */

int walk_path(const char *path, walker_fn callback, void *userdata,
              unsigned int flags)
{
    struct stat st;

    /* Check for unsupported features */
#ifndef STREAMIO_HAVE_LIBARCHIVE
    if (flags & WALK_EXPAND_ARCHIVES)
        return -ENOSYS;
#endif

    /* Check if any compression library is available if decompression requested */
    if (flags & WALK_DECOMPRESS) {
#if !defined(STREAMIO_HAVE_ZLIB) && !defined(STREAMIO_HAVE_BZIP2) && \
    !defined(STREAMIO_HAVE_LZMA) && !defined(STREAMIO_HAVE_ZSTD)
        return -ENOSYS;
#endif
    }

    if (stat(path, &st) < 0)
        return -errno;

    if (S_ISDIR(st.st_mode)) {
        return walk_directory_impl(path, callback, userdata, flags, 0);
    }

#ifdef STREAMIO_HAVE_LIBARCHIVE
    if ((flags & WALK_EXPAND_ARCHIVES) && is_archive(path)) {
        struct file_stream fs;

        if (file_stream_open(&fs, path, O_RDONLY, 0) < 0)
            return -errno;

        return walk_archive_impl(path, &fs.base, callback, userdata, flags, 0);
    }
#endif

    /* Single file - invoke callback if it passes filters */
    struct walker_entry entry = {
        .path = path,
        .name = /* basename */,
        .size = st.st_size,
        .mode = st.st_mode,
        .mtime = st.st_mtime,
        .is_dir = 0,
        .is_archive_entry = 0,
        .depth = 0,
    };

    /* Apply filters */
    if (flags & WALK_FILTER_FILES && S_ISDIR(st.st_mode))
        return 0;
    if (flags & WALK_FILTER_DIRS && !S_ISDIR(st.st_mode))
        return 0;

    /* Check capability requirements */
    if (flags & (WALK_REQUIRE_SEEK_FWD | WALK_REQUIRE_SEEK_BWD |
                 WALK_REQUIRE_SEEK_ABS | WALK_REQUIRE_MMAP |
                 WALK_REQUIRE_MMAP_NATIVE | WALK_REQUIRE_SIZE)) {
        if (!walker_entry_check_caps(&entry, flags))
            return 0;
    }

    return callback(&entry, userdata);
}
```

### 6.3 Compression Stream with Feature Detection

```c
/* compression_stream.c */

int compression_stream_init(struct compression_stream *this,
                             struct stream *underlying,
                             enum compression_type type,
                             int flags,
                             int owns_underlying)
{
    if (!compression_is_available(type))
        return -ENOSYS;

    /* Initialize based on type */
    switch (type) {
#ifdef STREAMIO_HAVE_ZLIB
    case COMPRESS_GZIP:
        return gzip_stream_init_internal(this, underlying, flags, owns_underlying);
    case COMPRESS_ZLIB:
        return zlib_stream_init_internal(this, underlying, flags, owns_underlying);
#endif

#ifdef STREAMIO_HAVE_BZIP2
    case COMPRESS_BZIP2:
        return bzip2_stream_init_internal(this, underlying, flags, owns_underlying);
#endif

#ifdef STREAMIO_HAVE_LZMA
    case COMPRESS_XZ:
        return xz_stream_init_internal(this, underlying, flags, owns_underlying);
#endif

#ifdef STREAMIO_HAVE_ZSTD
    case COMPRESS_ZSTD:
        return zstd_stream_init_internal(this, underlying, flags, owns_underlying);
#endif

    default:
        return -ENOTSUP;
    }
}

/* Auto-detect from magic bytes */
int compression_stream_auto(struct compression_stream *this,
                             struct stream *underlying,
                             int owns_underlying)
{
    unsigned char magic[16];
    ssize_t nread;
    enum compression_type type = COMPRESS_NONE;

    /* Read magic bytes */
    nread = stream_read(underlying, magic, sizeof(magic));
    if (nread < 0)
        return nread;

    /* Seek back */
    if (stream_seek(underlying, 0, SEEK_SET) < 0)
        return -EIO;

    /* Detect compression type */
    if (nread >= 2 && magic[0] == 0x1f && magic[1] == 0x8b) {
#ifdef STREAMIO_HAVE_ZLIB
        type = COMPRESS_GZIP;
#else
        return -ENOTSUP;
#endif
    } else if (nread >= 3 && magic[0] == 'B' && magic[1] == 'Z' && magic[2] == 'h') {
#ifdef STREAMIO_HAVE_BZIP2
        type = COMPRESS_BZIP2;
#else
        return -ENOTSUP;
#endif
    } else if (nread >= 6 && magic[0] == 0xFD && magic[1] == '7' &&
               magic[2] == 'z' && magic[3] == 'X' && magic[4] == 'Z' && magic[5] == 0x00) {
#ifdef STREAMIO_HAVE_LZMA
        type = COMPRESS_XZ;
#else
        return -ENOTSUP;
#endif
    } else if (nread >= 4 && magic[0] == 0x28 && magic[1] == 0xB5 &&
               magic[2] == 0x2F && magic[3] == 0xFD) {
#ifdef STREAMIO_HAVE_ZSTD
        type = COMPRESS_ZSTD;
#else
        return -ENOTSUP;
#endif
    } else {
        /* Not compressed */
        return -EINVAL;
    }

    return compression_stream_init(this, underlying, type, O_RDONLY, owns_underlying);
}
```

### 6.4 mmap Support with Start Offset

```c
/* stream.c */

void *stream_mmap(struct stream *this, off64_t start, size_t length, int prot)
{
    if (!this->ops->mmap) {
        /* Try emulated mmap if supported */
        if (this->caps & STREAM_CAP_MMAP_EMULATED)
            return stream_mmap_emulated(this, start, length, prot);

        this->error = ENOSYS;
        return NULL;
    }

    void *ptr = this->ops->mmap(this, start, length, prot);
    if (!ptr)
        this->error = errno;

    return ptr;
}

/* Emulated mmap - allocate buffer and read */
static void *stream_mmap_emulated(struct stream *this, off64_t start,
                                   size_t length, int prot)
{
    void *buf;
    off64_t old_pos;
    ssize_t nread;

    /* Only support read-only emulated mmap */
    if (prot & PROT_WRITE) {
        this->error = ENOTSUP;
        return NULL;
    }

    /* Allocate buffer */
    buf = malloc(length);
    if (!buf) {
        this->error = ENOMEM;
        return NULL;
    }

    /* Save current position */
    old_pos = stream_tell(this);

    /* Seek to start offset */
    if (stream_seek(this, start, SEEK_SET) < 0) {
        free(buf);
        return NULL;
    }

    /* Read data */
    nread = stream_read(this, buf, length);
    if (nread < 0) {
        free(buf);
        return NULL;
    }

    /* Restore position if possible */
    if (old_pos >= 0)
        stream_seek(this, old_pos, SEEK_SET);

    /* Zero-fill remainder if we read less than requested */
    if ((size_t)nread < length)
        memset((char *)buf + nread, 0, length - nread);

    return buf;
}

/* file_stream.c - native mmap with offset */
static void *file_stream_mmap_impl(void *this_ptr, off64_t start,
                                    size_t length, int prot)
{
    struct file_stream *this = this_ptr;

#ifdef _WIN32
    DWORD protect = PAGE_READONLY;
    DWORD access = FILE_MAP_READ;

    if (prot & PROT_WRITE) {
        protect = PAGE_READWRITE;
        access = FILE_MAP_WRITE;
    }

    /* Create file mapping */
    this->mapping_handle = CreateFileMapping(this->handle, NULL, protect,
                                              (DWORD)(start >> 32),
                                              (DWORD)(start & 0xFFFFFFFF),
                                              NULL);
    if (!this->mapping_handle)
        return NULL;

    /* Map view at offset */
    void *addr = MapViewOfFile(this->mapping_handle, access,
                                (DWORD)(start >> 32),
                                (DWORD)(start & 0xFFFFFFFF),
                                length);

    if (!addr) {
        CloseHandle(this->mapping_handle);
        this->mapping_handle = NULL;
        return NULL;
    }

#else /* POSIX */
    int prot_flags = 0;

    if (prot & PROT_READ)
        prot_flags |= PROT_READ;
    if (prot & PROT_WRITE)
        prot_flags |= PROT_WRITE;

    void *addr = mmap(NULL, length, prot_flags, MAP_PRIVATE, this->fd, start);
    if (addr == MAP_FAILED)
        return NULL;
#endif

    this->mmap_addr = addr;
    this->mmap_size = length;
    this->mmap_start = start;

    return addr;
}
```

### 6.5 Capability Query Helpers

```c
/* stream.c */

int stream_can_seek_forward(struct stream *this)
{
    return !!(stream_get_caps(this) & STREAM_CAP_SEEK_CUR);
}

int stream_can_seek_backward(struct stream *this)
{
    /* Some streams can seek forward but not backward (e.g., compressed) */
    unsigned int caps = stream_get_caps(this);

    /* If we can seek absolute or relative to end, we can seek backward */
    if (caps & (STREAM_CAP_SEEK_SET | STREAM_CAP_SEEK_END))
        return 1;

    /* Otherwise, query the stream implementation */
    /* Compression streams typically can't seek backward */
    if (caps & STREAM_CAP_COMPRESSED)
        return 0;

    return !!(caps & STREAM_CAP_SEEK_CUR);
}

int stream_can_seek_to_start(struct stream *this)
{
    unsigned int caps = stream_get_caps(this);
    return !!(caps & (STREAM_CAP_SEEK_SET | STREAM_CAP_SEEK_CUR));
}

int stream_can_seek_to_end(struct stream *this)
{
    unsigned int caps = stream_get_caps(this);
    return !!(caps & STREAM_CAP_SEEK_END);
}

int stream_can_mmap_native(struct stream *this)
{
    return !!(stream_get_caps(this) & STREAM_CAP_MMAP);
}
```

## 7. Build System

### 7.1 CMake Configuration

```cmake
cmake_minimum_required(VERSION 3.10)
project(streamio VERSION 1.1.0 LANGUAGES C)

# Options for optional features
option(STREAMIO_ENABLE_ZLIB "Enable gzip/zlib support" ON)
option(STREAMIO_ENABLE_BZIP2 "Enable bzip2 support" ON)
option(STREAMIO_ENABLE_LZMA "Enable xz/lzma support" ON)
option(STREAMIO_ENABLE_ZSTD "Enable zstd support" ON)
option(STREAMIO_ENABLE_LIBARCHIVE "Enable archive support" ON)
option(STREAMIO_BUILD_TESTS "Build test suite" ON)
option(STREAMIO_BUILD_EXAMPLES "Build examples" ON)

# Large file support
add_definitions(-D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64)

# Find optional dependencies
set(STREAMIO_HAVE_ZLIB 0)
set(STREAMIO_HAVE_BZIP2 0)
set(STREAMIO_HAVE_LZMA 0)
set(STREAMIO_HAVE_ZSTD 0)
set(STREAMIO_HAVE_LIBARCHIVE 0)

if(STREAMIO_ENABLE_ZLIB)
    find_package(ZLIB)
    if(ZLIB_FOUND)
        set(STREAMIO_HAVE_ZLIB 1)
        message(STATUS "zlib support: ENABLED")
    else()
        message(STATUS "zlib support: DISABLED (library not found)")
    endif()
else()
    message(STATUS "zlib support: DISABLED (user option)")
endif()

if(STREAMIO_ENABLE_BZIP2)
    find_package(BZip2)
    if(BZIP2_FOUND)
        set(STREAMIO_HAVE_BZIP2 1)
        message(STATUS "bzip2 support: ENABLED")
    else()
        message(STATUS "bzip2 support: DISABLED (library not found)")
    endif()
else()
    message(STATUS "bzip2 support: DISABLED (user option)")
endif()

if(STREAMIO_ENABLE_LZMA)
    find_package(LibLZMA)
    if(LIBLZMA_FOUND)
        set(STREAMIO_HAVE_LZMA 1)
        message(STATUS "lzma support: ENABLED")
    else()
        message(STATUS "lzma support: DISABLED (library not found)")
    endif()
else()
    message(STATUS "lzma support: DISABLED (user option)")
endif()

if(STREAMIO_ENABLE_ZSTD)
    find_package(zstd)
    if(zstd_FOUND)
        set(STREAMIO_HAVE_ZSTD 1)
        message(STATUS "zstd support: ENABLED")
    else()
        message(STATUS "zstd support: DISABLED (library not found)")
    endif()
else()
    message(STATUS "zstd support: DISABLED (user option)")
endif()

if(STREAMIO_ENABLE_LIBARCHIVE)
    find_package(LibArchive)
    if(LibArchive_FOUND)
        set(STREAMIO_HAVE_LIBARCHIVE 1)
        message(STATUS "libarchive support: ENABLED")
    else()
        message(STATUS "libarchive support: DISABLED (library not found)")
    endif()
else()
    message(STATUS "libarchive support: DISABLED (user option)")
endif()

# Generate config header
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/streamio_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/streamio_config.h
)

# Core sources (always compiled)
set(STREAMIO_CORE_SOURCES
    src/stream.c
    src/file_stream.c
    src/mem_stream.c
    src/walker.c
    src/util.c
)

# Optional sources
set(STREAMIO_OPTIONAL_SOURCES "")

if(STREAMIO_HAVE_ZLIB OR STREAMIO_HAVE_BZIP2 OR STREAMIO_HAVE_LZMA OR STREAMIO_HAVE_ZSTD)
    list(APPEND STREAMIO_OPTIONAL_SOURCES src/compression_stream.c)
endif()

if(STREAMIO_HAVE_LIBARCHIVE)
    list(APPEND STREAMIO_OPTIONAL_SOURCES src/archive_stream.c)
endif()

# Create library
add_library(streamio
    ${STREAMIO_CORE_SOURCES}
    ${STREAMIO_OPTIONAL_SOURCES}
)

# Include directories
target_include_directories(streamio PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Link optional libraries
if(STREAMIO_HAVE_ZLIB)
    target_link_libraries(streamio PUBLIC ZLIB::ZLIB)
endif()

if(STREAMIO_HAVE_BZIP2)
    target_link_libraries(streamio PUBLIC BZip2::BZip2)
endif()

if(STREAMIO_HAVE_LZMA)
    target_link_libraries(streamio PUBLIC LibLZMA::LibLZMA)
endif()

if(STREAMIO_HAVE_ZSTD)
    target_link_libraries(streamio PUBLIC zstd::zstd)
endif()

if(STREAMIO_HAVE_LIBARCHIVE)
    target_link_libraries(streamio PUBLIC LibArchive::LibArchive)
endif()

# Platform-specific libraries
if(WIN32)
    # No additional libraries needed on Windows
else()
    # POSIX systems may need -lm for math functions
    target_link_libraries(streamio PUBLIC m)
endif()

# Set compiler flags
target_compile_options(streamio PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra -pedantic>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)

# Installation
install(TARGETS streamio
    EXPORT streamio-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/streamio_config.h
    DESTINATION include
)

# Export targets
install(EXPORT streamio-targets
    FILE streamio-config.cmake
    NAMESPACE streamio::
    DESTINATION lib/cmake/streamio
)

# Tests
if(STREAMIO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Examples
if(STREAMIO_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
```

### 7.2 Config Header Template

```c
/* include/streamio_config.h.in */
#ifndef STREAMIO_CONFIG_H
#define STREAMIO_CONFIG_H

/* Version information */
#define STREAMIO_VERSION_MAJOR @PROJECT_VERSION_MAJOR@
#define STREAMIO_VERSION_MINOR @PROJECT_VERSION_MINOR@
#define STREAMIO_VERSION_PATCH @PROJECT_VERSION_PATCH@
#define STREAMIO_VERSION_STRING "@PROJECT_VERSION@"

/* Optional feature support - set by CMake */
#cmakedefine STREAMIO_HAVE_ZLIB @STREAMIO_HAVE_ZLIB@
#cmakedefine STREAMIO_HAVE_BZIP2 @STREAMIO_HAVE_BZIP2@
#cmakedefine STREAMIO_HAVE_LZMA @STREAMIO_HAVE_LZMA@
#cmakedefine STREAMIO_HAVE_ZSTD @STREAMIO_HAVE_ZSTD@
#cmakedefine STREAMIO_HAVE_LIBARCHIVE @STREAMIO_HAVE_LIBARCHIVE@

#endif /* STREAMIO_CONFIG_H */
```

## 8. Usage Examples

### 8.1 Feature Detection

```c
#include <streamio.h>
#include <stdio.h>

int main(void)
{
    printf("streamio version: %s\n", streamio_get_version());
    printf("Available features: %s\n", streamio_get_features_string());

    if (streamio_has_feature(STREAMIO_FEAT_ZLIB))
        printf("  - gzip compression available\n");

    if (streamio_has_feature(STREAMIO_FEAT_LIBARCHIVE))
        printf("  - archive support available\n");

    /* Check specific compression types */
    if (compression_is_available(COMPRESS_GZIP))
        printf("  - Can handle gzip files\n");

    if (compression_is_available(COMPRESS_BZIP2))
        printf("  - Can handle bzip2 files\n");

    return 0;
}
```

### 8.2 Conditional Compression Support

```c
#include <streamio.h>
#include <streamio_config.h>

int read_possibly_compressed_file(const char *path, void **data, size_t *size)
{
    struct file_stream fs;
    struct stream *s = &fs.base;
    int ret;

    /* Open file */
    ret = file_stream_open(&fs, path, O_RDONLY, 0);
    if (ret < 0)
        return ret;

#ifdef STREAMIO_HAVE_ZLIB
    /* Try to auto-detect compression */
    struct compression_stream cs;
    if (compression_stream_auto(&cs, &fs.base, 1) == 0) {
        /* File is compressed, use decompression stream */
        s = &cs.base;
    }
#endif

    /* Read entire stream */
    ret = stream_read_all(s, data, size);

    stream_close(s);
    return ret;
}
```

### 8.3 Walk with Capability Requirements

```c
#include <streamio.h>

int process_seekable_entry(const struct walker_entry *entry, void *data)
{
    struct stream *s;

    printf("Processing: %s\n", entry->path);

    /* Open stream - guaranteed to be seekable due to walk flags */
    if (walker_entry_open(entry, &s) < 0)
        return 0;

    /* Can safely use seeking operations */
    off64_t size = stream_size(s);
    stream_seek(s, 0, SEEK_SET);

    /* Process file... */

    stream_close(s);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <path>\n", argv[0]);
        return 1;
    }

    /* Walk only files that support seeking and mmap */
    unsigned int flags = WALK_RECURSE_DIRS |
                         WALK_REQUIRE_SEEK_ABS |
                         WALK_REQUIRE_MMAP |
                         WALK_FILTER_FILES;

    int ret = walk_path(argv[1], process_seekable_entry, NULL, flags);

    if (ret == -ENOSYS) {
        fprintf(stderr, "Required features not available\n");
        return 1;
    }

    return ret;
}
```

### 8.4 Graceful Degradation

```c
#include <streamio.h>
#include <streamio_config.h>

int process_archive(const char *path)
{
#ifdef STREAMIO_HAVE_LIBARCHIVE
    struct file_stream fs;
    struct archive_stream ar;

    if (file_stream_open(&fs, path, O_RDONLY, 0) < 0) {
        fprintf(stderr, "Failed to open %s\n", path);
        return -1;
    }

    if (archive_stream_open(&ar, &fs.base, 1) < 0) {
        fprintf(stderr, "Failed to open as archive\n");
        stream_close(&fs.base);
        return -1;
    }

    /* Process archive entries... */

    stream_close(&ar.base);
    return 0;

#else
    fprintf(stderr, "Archive support not available\n");
    fprintf(stderr, "Please rebuild with STREAMIO_ENABLE_LIBARCHIVE=ON\n");
    return -ENOSYS;
#endif
}
```

### 8.5 mmap with Offset for Large Files

```c
#include <streamio.h>

int process_large_file_in_chunks(const char *path)
{
    struct file_stream fs;
    off64_t file_size;
    size_t chunk_size = 100 * 1024 * 1024;  /* 100MB chunks */

    if (file_stream_open(&fs, path, O_RDONLY, 0) < 0)
        return -1;

    file_size = stream_size(&fs.base);

    /* Process file in chunks */
    for (off64_t offset = 0; offset < file_size; offset += chunk_size) {
        size_t len = (offset + chunk_size > file_size) ?
                     (file_size - offset) : chunk_size;

        void *ptr = stream_mmap(&fs.base, offset, len, PROT_READ);
        if (!ptr) {
            fprintf(stderr, "mmap failed at offset %lld\n", (long long)offset);
            stream_close(&fs.base);
            return -1;
        }

        /* Process mapped region */
        process_data(ptr, len);

        stream_munmap(&fs.base, ptr, len);
    }

    stream_close(&fs.base);
    return 0;
}
```

### 8.6 Walk Archive with Decompression

```c
#include <streamio.h>

int print_entry(const struct walker_entry *entry, void *data)
{
    printf("%*s%s", entry->depth * 2, "", entry->name);

    if (entry->is_dir)
        printf("/");
    else
        printf(" (%lld bytes)", (long long)entry->size);

    printf("\n");

    return 0;  /* Continue */
}

int main(int argc, char **argv)
{
    if (argc < 2)
        return 1;

    unsigned int flags = WALK_EXPAND_ARCHIVES | WALK_DECOMPRESS | WALK_RECURSE_DIRS;

    int ret = walk_path(argv[1], print_entry, NULL, flags);

    if (ret == -ENOSYS) {
        fprintf(stderr, "Archive or compression support not available.\n");
        fprintf(stderr, "Available features: %s\n", streamio_get_features_string());
        return 1;
    }

    return ret < 0 ? 1 : 0;
}
```

## 9. Testing Strategy

### 9.1 Feature-Conditional Tests

```cmake
# tests/CMakeLists.txt

add_executable(test_core
    test_stream.c
    test_file_stream.c
    test_mem_stream.c
)
target_link_libraries(test_core streamio)
add_test(NAME core_tests COMMAND test_core)

if(STREAMIO_HAVE_ZLIB OR STREAMIO_HAVE_BZIP2 OR STREAMIO_HAVE_LZMA OR STREAMIO_HAVE_ZSTD)
    add_executable(test_compression
        test_compression.c
    )
    target_link_libraries(test_compression streamio)
    add_test(NAME compression_tests COMMAND test_compression)
endif()

if(STREAMIO_HAVE_LIBARCHIVE)
    add_executable(test_archive
        test_archive.c
    )
    target_link_libraries(test_archive streamio)
    add_test(NAME archive_tests COMMAND test_archive)
endif()
```

### 9.2 Runtime Feature Testing

```c
/* tests/test_compression.c */
#include <streamio.h>
#include <streamio_config.h>
#include <assert.h>

void test_gzip_compression(void)
{
#ifdef STREAMIO_HAVE_ZLIB
    assert(compression_is_available(COMPRESS_GZIP));

    /* Run gzip-specific tests */
    /* ... */
#else
    printf("SKIP: gzip tests (not compiled in)\n");
#endif
}

void test_bzip2_compression(void)
{
#ifdef STREAMIO_HAVE_BZIP2
    assert(compression_is_available(COMPRESS_BZIP2));

    /* Run bzip2-specific tests */
    /* ... */
#else
    printf("SKIP: bzip2 tests (not compiled in)\n");
#endif
}

int main(void)
{
    printf("Testing with features: %s\n", streamio_get_features_string());

    test_gzip_compression();
    test_bzip2_compression();

    return 0;
}
```

## 10. Documentation

### 10.1 Feature Matrix Documentation

```markdown
# Feature Matrix

| Feature          | Required Libraries | CMake Option              | Runtime Detection       |
|------------------|--------------------|---------------------------|-------------------------|
| gzip/zlib        | zlib               | STREAMIO_ENABLE_ZLIB      | STREAMIO_FEAT_ZLIB      |
| bzip2            | libbz2             | STREAMIO_ENABLE_BZIP2     | STREAMIO_FEAT_BZIP2     |
| xz/lzma          | liblzma            | STREAMIO_ENABLE_LZMA      | STREAMIO_FEAT_LZMA      |
| zstd             | libzstd            | STREAMIO_ENABLE_ZSTD      | STREAMIO_FEAT_ZSTD      |
| Archive support  | libarchive         | STREAMIO_ENABLE_LIBARCHIVE| STREAMIO_FEAT_LIBARCHIVE|

## Building with Minimal Dependencies

To build with only core functionality (no compression or archive support):

```bash
cmake -DSTREAMIO_ENABLE_ZLIB=OFF \
      -DSTREAMIO_ENABLE_BZIP2=OFF \
      -DSTREAMIO_ENABLE_LZMA=OFF \
      -DSTREAMIO_ENABLE_ZSTD=OFF \
      -DSTREAMIO_ENABLE_LIBARCHIVE=OFF \
      ..
```

## Detecting Available Features at Runtime

```c
if (streamio_has_feature(STREAMIO_FEAT_ZLIB)) {
    /* Use gzip functionality */
} else {
    /* Provide alternative or error message */
}
```
```
