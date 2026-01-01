# Basic tests
add_executable(test_basic test_basic.c)
target_link_libraries(test_basic stream)
add_test(NAME basic_tests COMMAND test_basic)

# Walker tests
add_executable(test_walker test_walker.c)
target_link_libraries(test_walker stream)
add_test(NAME walker_tests COMMAND test_walker)

# Compression tests
if(STREAM_HAVE_ZLIB)
    add_executable(test_compression test_compression.c)
    target_link_libraries(test_compression stream)
    add_test(NAME compression_tests COMMAND test_compression)
endif()

# Archive tests
if(STREAM_HAVE_LIBARCHIVE)
    add_executable(test_archive test_archive.c)
    target_link_libraries(test_archive stream)
    add_test(NAME archive_tests COMMAND test_archive)
endif()
