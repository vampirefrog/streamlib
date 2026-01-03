/**
 * @file file_stream.c
 * @brief File stream implementation (POSIX and Windows)
 */

#include "stream.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#ifndef _WIN32
/* POSIX headers */
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

/* O_LARGEFILE is Linux-specific; on macOS/BSD large files are the default */
#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif
#else
/* Windows headers */
#include <windows.h>
#include <io.h>
#endif

/* Forward declarations */
static ssize_t file_stream_read_impl(void *stream, void *buf, size_t count);
static ssize_t file_stream_write_impl(void *stream, const void *buf, size_t count);
static off64_t file_stream_seek_impl(void *stream, off64_t offset, int whence);
static off64_t file_stream_tell_impl(void *stream);
static off64_t file_stream_size_impl(void *stream);
static void *file_stream_mmap_impl(void *stream, off64_t start, size_t length,
				   int prot);
static int file_stream_munmap_impl(void *stream, void *addr, size_t length);
static int file_stream_flush_impl(void *stream);
static int file_stream_close_impl(void *stream);

/* Virtual method table */
static const struct stream_ops file_stream_ops = {
	.read = file_stream_read_impl,
	.write = file_stream_write_impl,
	.seek = file_stream_seek_impl,
	.tell = file_stream_tell_impl,
	.size = file_stream_size_impl,
	.mmap = file_stream_mmap_impl,
	.munmap = file_stream_munmap_impl,
	.flush = file_stream_flush_impl,
	.close = file_stream_close_impl,
	.vprintf = NULL,  /* Use default */
	.get_caps = NULL,  /* Use cached caps */
};

/* Open file stream */
int file_stream_open(struct file_stream *stream, const char *path, int flags,
		     mode_t mode)
{
	memset(stream, 0, sizeof(*stream));

#ifndef _WIN32
	/* POSIX implementation */
	stream->fd = open(path, flags | O_LARGEFILE, mode);
	if (stream->fd < 0)
		return -errno;
#else
	/* Windows implementation */
	DWORD desired_access = 0;
	DWORD creation_disposition = 0;
	DWORD share_mode = FILE_SHARE_READ;

	/* Map POSIX flags to Windows flags */
	int acc_mode = flags & O_ACCMODE;
	if (acc_mode == O_RDONLY) {
		desired_access = GENERIC_READ;
		creation_disposition = OPEN_EXISTING;
	} else if (acc_mode == O_WRONLY) {
		desired_access = GENERIC_WRITE;
		creation_disposition = (flags & O_CREAT) ?
			((flags & O_TRUNC) ? CREATE_ALWAYS : OPEN_ALWAYS) : OPEN_EXISTING;
	} else if (acc_mode == O_RDWR) {
		desired_access = GENERIC_READ | GENERIC_WRITE;
		creation_disposition = (flags & O_CREAT) ?
			((flags & O_TRUNC) ? CREATE_ALWAYS : OPEN_ALWAYS) : OPEN_EXISTING;
	}

	/* O_TRUNC without O_CREAT means truncate existing file */
	if ((flags & O_TRUNC) && !(flags & O_CREAT))
		creation_disposition = TRUNCATE_EXISTING;

	stream->handle = CreateFileA(path, desired_access, share_mode,
				     NULL, creation_disposition,
				     FILE_ATTRIBUTE_NORMAL, NULL);

	if (stream->handle == INVALID_HANDLE_VALUE) {
		/* Map Windows error to errno */
		DWORD err = GetLastError();
		if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND)
			return -ENOENT;
		if (err == ERROR_ACCESS_DENIED)
			return -EACCES;
		return -EIO;
	}

	(void)mode;  /* Windows doesn't use POSIX mode */
#endif

	/* Determine capabilities */
	unsigned int caps = STREAM_CAP_SEEK_SET | STREAM_CAP_SEEK_CUR |
			    STREAM_CAP_SEEK_END | STREAM_CAP_TELL |
			    STREAM_CAP_SIZE | STREAM_CAP_MMAP |
			    STREAM_CAP_FLUSH;

	if ((flags & O_ACCMODE) == O_RDONLY || (flags & O_ACCMODE) == O_RDWR)
		caps |= STREAM_CAP_READ;

	if ((flags & O_ACCMODE) == O_WRONLY || (flags & O_ACCMODE) == O_RDWR)
		caps |= STREAM_CAP_WRITE;

	stream_init(&stream->base, &file_stream_ops, flags, caps);

	/* Save path for debugging */
	stream->path = strdup(path);

	stream->mmap_addr = NULL;
	stream->mmap_size = 0;
	stream->mmap_start = 0;

	return 0;
}

/* Initialize from existing fd */
int file_stream_from_fd(struct file_stream *stream, int fd, int flags)
{
	memset(stream, 0, sizeof(*stream));

#ifndef _WIN32
	stream->fd = fd;
#else
	/* On Windows, convert fd to HANDLE */
	stream->handle = (HANDLE)_get_osfhandle(fd);
	if (stream->handle == INVALID_HANDLE_VALUE)
		return -EBADF;
#endif

	/* Determine capabilities */
	unsigned int caps = STREAM_CAP_SEEK_SET | STREAM_CAP_SEEK_CUR |
			    STREAM_CAP_SEEK_END | STREAM_CAP_TELL |
			    STREAM_CAP_SIZE | STREAM_CAP_MMAP |
			    STREAM_CAP_FLUSH;

	if ((flags & O_ACCMODE) == O_RDONLY || (flags & O_ACCMODE) == O_RDWR)
		caps |= STREAM_CAP_READ;

	if ((flags & O_ACCMODE) == O_WRONLY || (flags & O_ACCMODE) == O_RDWR)
		caps |= STREAM_CAP_WRITE;

	stream_init(&stream->base, &file_stream_ops, flags, caps);

	stream->path = NULL;
	stream->mmap_addr = NULL;
	stream->mmap_size = 0;
	stream->mmap_start = 0;

	return 0;
}

/* Read implementation */
static ssize_t file_stream_read_impl(void *stream_ptr, void *buf, size_t count)
{
	struct file_stream *stream = stream_ptr;
#ifndef _WIN32
	ssize_t ret = read(stream->fd, buf, count);
	if (ret < 0)
		return -errno;
	return ret;
#else
	DWORD bytes_read;
	if (!ReadFile(stream->handle, buf, (DWORD)count, &bytes_read, NULL))
		return -EIO;
	return (ssize_t)bytes_read;
#endif
}

/* Write implementation */
static ssize_t file_stream_write_impl(void *stream_ptr, const void *buf,
				      size_t count)
{
	struct file_stream *stream = stream_ptr;
#ifndef _WIN32
	ssize_t ret = write(stream->fd, buf, count);
	if (ret < 0)
		return -errno;
	return ret;
#else
	DWORD bytes_written;
	if (!WriteFile(stream->handle, buf, (DWORD)count, &bytes_written, NULL))
		return -EIO;
	return (ssize_t)bytes_written;
#endif
}

/* Seek implementation */
static off64_t file_stream_seek_impl(void *stream_ptr, off64_t offset, int whence)
{
	struct file_stream *stream = stream_ptr;
#ifndef _WIN32
	off64_t ret = lseek(stream->fd, offset, whence);
	if (ret < 0)
		return -errno;
	return ret;
#else
	DWORD move_method;
	switch (whence) {
	case SEEK_SET: move_method = FILE_BEGIN; break;
	case SEEK_CUR: move_method = FILE_CURRENT; break;
	case SEEK_END: move_method = FILE_END; break;
	default: return -EINVAL;
	}

	LARGE_INTEGER li_offset, li_new_pos;
	li_offset.QuadPart = offset;

	if (!SetFilePointerEx(stream->handle, li_offset, &li_new_pos, move_method))
		return -EIO;

	return (off64_t)li_new_pos.QuadPart;
#endif
}

/* Tell implementation */
static off64_t file_stream_tell_impl(void *stream_ptr)
{
	struct file_stream *stream = stream_ptr;
#ifndef _WIN32
	off64_t ret = lseek(stream->fd, 0, SEEK_CUR);
	if (ret < 0)
		return -errno;
	return ret;
#else
	LARGE_INTEGER li_offset, li_pos;
	li_offset.QuadPart = 0;

	if (!SetFilePointerEx(stream->handle, li_offset, &li_pos, FILE_CURRENT))
		return -EIO;

	return (off64_t)li_pos.QuadPart;
#endif
}

/* Size implementation */
static off64_t file_stream_size_impl(void *stream_ptr)
{
	struct file_stream *stream = stream_ptr;
#ifndef _WIN32
	struct stat st;

	if (fstat(stream->fd, &st) < 0)
		return -errno;

	return st.st_size;
#else
	LARGE_INTEGER li_size;

	if (!GetFileSizeEx(stream->handle, &li_size))
		return -EIO;

	return (off64_t)li_size.QuadPart;
#endif
}

/* mmap implementation */
static void *file_stream_mmap_impl(void *stream_ptr, off64_t start, size_t length,
				   int prot)
{
	struct file_stream *stream = stream_ptr;

	/* Unmap any existing mapping first */
	if (stream->mmap_addr) {
#ifndef _WIN32
		munmap(stream->mmap_addr, stream->mmap_size);
#else
		UnmapViewOfFile(stream->mmap_addr);
		if (stream->mapping_handle) {
			CloseHandle(stream->mapping_handle);
			stream->mapping_handle = NULL;
		}
#endif
		stream->mmap_addr = NULL;
	}

#ifndef _WIN32
	/* POSIX implementation */
	int prot_flags = 0;
	if (prot & PROT_READ)
		prot_flags |= PROT_READ;
	if (prot & PROT_WRITE)
		prot_flags |= PROT_WRITE;

	void *addr = mmap(NULL, length, prot_flags, MAP_PRIVATE, stream->fd, start);
	if (addr == MAP_FAILED)
		return NULL;
#else
	/* Windows implementation */
	DWORD protect = PAGE_READONLY;
	DWORD desired_access = FILE_MAP_READ;

	if (prot & PROT_WRITE) {
		protect = PAGE_READWRITE;
		desired_access = FILE_MAP_WRITE | FILE_MAP_READ;
	}

	/* Create file mapping */
	stream->mapping_handle = CreateFileMappingA(stream->handle, NULL,
						    protect, 0, 0, NULL);
	if (!stream->mapping_handle)
		return NULL;

	/* Map view of file */
	DWORD offset_high = (DWORD)(start >> 32);
	DWORD offset_low = (DWORD)(start & 0xFFFFFFFF);

	void *addr = MapViewOfFile(stream->mapping_handle, desired_access,
				   offset_high, offset_low, length);
	if (!addr) {
		CloseHandle(stream->mapping_handle);
		stream->mapping_handle = NULL;
		return NULL;
	}
#endif

	/* Save mapping info for cleanup */
	stream->mmap_addr = addr;
	stream->mmap_size = length;
	stream->mmap_start = start;

	return addr;
}

/* munmap implementation */
static int file_stream_munmap_impl(void *stream_ptr, void *addr, size_t length)
{
	struct file_stream *stream = stream_ptr;

	/* Verify this is our mapped region */
	if (addr != stream->mmap_addr || length != stream->mmap_size)
		return -EINVAL;

#ifndef _WIN32
	if (munmap(addr, length) < 0)
		return -errno;
#else
	if (!UnmapViewOfFile(addr))
		return -EIO;

	if (stream->mapping_handle) {
		CloseHandle(stream->mapping_handle);
		stream->mapping_handle = NULL;
	}
#endif

	stream->mmap_addr = NULL;
	stream->mmap_size = 0;
	stream->mmap_start = 0;

	return 0;
}

/* Flush implementation */
static int file_stream_flush_impl(void *stream_ptr)
{
	struct file_stream *stream = stream_ptr;

#ifndef _WIN32
	if (fsync(stream->fd) < 0)
		return -errno;
#else
	if (!FlushFileBuffers(stream->handle))
		return -EIO;
#endif

	return 0;
}

/* Close implementation */
static int file_stream_close_impl(void *stream_ptr)
{
	struct file_stream *stream = stream_ptr;

	/* Unmap if needed */
	if (stream->mmap_addr) {
#ifndef _WIN32
		munmap(stream->mmap_addr, stream->mmap_size);
#else
		UnmapViewOfFile(stream->mmap_addr);
		if (stream->mapping_handle) {
			CloseHandle(stream->mapping_handle);
			stream->mapping_handle = NULL;
		}
#endif
		stream->mmap_addr = NULL;
	}

	/* Close file handle */
#ifndef _WIN32
	if (stream->fd >= 0) {
		close(stream->fd);
		stream->fd = -1;
	}
#else
	if (stream->handle != INVALID_HANDLE_VALUE) {
		CloseHandle(stream->handle);
		stream->handle = INVALID_HANDLE_VALUE;
	}
#endif

	/* Free path */
	if (stream->path) {
		free(stream->path);
		stream->path = NULL;
	}

	return 0;
}
