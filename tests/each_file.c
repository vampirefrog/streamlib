#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../stream.h"

// Mock callback function to count the number of times it's called
int mock_file_callback(const char *full_path, struct stream *stream, void *user_data) {
    int *count = (int *)user_data;
    (*count)++;
    return 0; // Return 0 to indicate success
}

void test_each_file_simple(void) {
    int callback_count = 0;
    struct file_type_filter filters[] = {
        {".txt", mock_file_callback, &callback_count},
        {NULL, NULL, NULL} // End of filter list
    };

    int result = each_file("test_directory", filters, 0);
    assert(result == 0);
    assert(callback_count == 0);
}

void test_each_file_with_flags(void) {
    struct file_type_filter filters[] = {
        {".txt", mock_file_callback, NULL},
        {NULL, NULL, NULL} // End of filter list
    };

    int callback_count = 0;
    filters[0].user_data = &callback_count;

    int result = each_file("test_directory", filters, EF_RECURSE_DIRS);
    assert(result == 0);
    assert(callback_count == 5);
}

void test_each_file_no_matching_files(void) {
    struct file_type_filter filters[] = {
        {".nonexistent", mock_file_callback, NULL},
        {NULL, NULL, NULL} // End of filter list
    };

    int callback_count = 0;
    filters[0].user_data = &callback_count;

    int result = each_file("test_directory", filters, 0);
    assert(result == 0);
    assert(callback_count == 0);
}

void test_each_file_multiple_filters(void) {
    struct file_type_filter filters[] = {
        {".txt", mock_file_callback, NULL},
        {".jpg", mock_file_callback, NULL},
        {NULL, NULL, NULL} // End of filter list
    };

    int callback_count = 0;
    filters[0].user_data = &callback_count;
    filters[1].user_data = &callback_count;

    int result = each_file("test_directory", filters, EF_RECURSE_DIRS);
    assert(result == 0);
    assert(callback_count == 6);
}

int main() {
    test_each_file_simple();
    test_each_file_with_flags();
    test_each_file_no_matching_files();
    test_each_file_multiple_filters();

    printf("All tests passed.\n");
    return 0;
}
