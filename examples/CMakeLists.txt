# Read file example
add_executable(read_file read_file.c)
target_link_libraries(read_file stream)

# Walk tree example
add_executable(walk_tree walk_tree.c)
target_link_libraries(walk_tree stream)

# Large file mmap example
add_executable(large_file_mmap large_file_mmap.c)
target_link_libraries(large_file_mmap stream)

# Read gzip example
if(HAVE_ZLIB)
    add_executable(read_gzip read_gzip.c)
    target_link_libraries(read_gzip stream)

    # Test mmap emulation on compressed streams
    add_executable(test_mmap_emulation test_mmap_emulation.c)
    target_link_libraries(test_mmap_emulation stream)
endif()

# List archive example
if(HAVE_LIBARCHIVE)
    add_executable(list_archive list_archive.c)
    target_link_libraries(list_archive stream)
endif()

# MIDI file generator (in-memory track generation)
add_executable(midi_generator midi_generator.c)
target_link_libraries(midi_generator stream)

# VGZ analyzer (recursive archive + decompression)
if(HAVE_LIBARCHIVE AND HAVE_ZLIB)
    add_executable(vgz_analyzer vgz_analyzer.c)
    target_link_libraries(vgz_analyzer stream)
endif()

# Binary file writer (endianness control with compression)
if(HAVE_ZLIB)
    add_executable(binary_writer binary_writer.c)
    target_link_libraries(binary_writer stream)
endif()
