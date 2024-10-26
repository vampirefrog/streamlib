#pragma once

#include "stream.h"

/**
 * @struct path_info
 * @brief A set of paths which you receive for each file.
 * When the zip_file_name is NULL, file_name refers to the real file on the
 * filesystem.
 * When it is not NULL, it refers to the file inside the zip file.
 */
struct path_info {
#ifdef HAVE_LIBZIP
	char *zip_file_name;    // foo/bar/baz.zip
	char *zip_file_dirname; // foo/bar
	char *zip_file_base;    // baz
#endif

	char *file_name;     // foo/bar/baz.txt
	char *file_dirname;  // foo/bar
	char *file_basename; // baz.txt
	char *file_base;     // baz
	char *file_ext;      // txt
};
#ifdef WIN32
struct path_infow {
#ifdef HAVE_LIBZIP
	wchar_t *zip_file_name;    // foo/bar/baz.zip
	wchar_t *zip_file_dirname; // foo/bar
	wchar_t *zip_file_base;    // baz
#endif

	wchar_t *file_name;     // foo/bar/baz.txt
	wchar_t *file_dirname;  // foo/bar
	wchar_t *file_basename; // baz.txt
	wchar_t *file_base;     // baz
	wchar_t *file_ext;      // txt
};
#endif

/**
 * @struct file_type_filter
 * @brief Structure for filtering files based on their type.
 */
struct file_type_filter {
	const char *ext; /**< File extension to filter */
	int (*file_cb)(struct path_info *path_info, struct stream *stream, void *user_data); /**< Callback function for processing the file */
	void *user_data; /**< User data for the callback function */
};
#ifdef WIN32
struct file_type_filterw {
	const wchar_t *ext; /**< File extension to filter */
	int (*file_cb)(struct path_infow *path_info, struct stream *stream, void *user_data); /**< Callback function for processing the file */
	void *user_data; /**< User data for the callback function */
};
#endif

#define EF_RECURSE_DIRS 0x01 /**< Flag to recurse directories */
#ifdef HAVE_LIBZIP
#define EF_RECURSE_ARCHIVES 0x02 /**< Flag to recurse archives (currently zip only) */
#endif
#define EF_OPEN_STREAM 0x04 /**< Flag to open the file and provide the stream instance */

/**
 * @brief Process each file in the specified path according to the filters and flags.
 * @param path Path to process.
 * @param filters Null terminated list of extensions to process.
 * @param flags Flags to control the processing.
 * @return Status code.
 */
int each_file(const char *path, struct file_type_filter *filters, int flags);
#ifdef WIN32
int each_filew(const wchar_t *path, struct file_type_filterw *filters, int flags);
#endif
