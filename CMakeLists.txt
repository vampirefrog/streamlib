cmake_minimum_required(VERSION 3.10)
project(streamlib VERSION 1.0.0 LANGUAGES C)

# Options for optional features
option(ENABLE_ZLIB "Enable gzip/zlib support" ON)
option(ENABLE_BZIP2 "Enable bzip2 support" ON)
option(ENABLE_LZMA "Enable xz/lzma support" ON)
option(ENABLE_ZSTD "Enable zstd support" ON)
option(ENABLE_LIBARCHIVE "Enable archive support" ON)
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_EXAMPLES "Build examples" ON)

# C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Large file support
add_definitions(-D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64)

# Find optional dependencies
set(HAVE_ZLIB 0)
set(HAVE_BZIP2 0)
set(HAVE_LZMA 0)
set(HAVE_ZSTD 0)
set(HAVE_LIBARCHIVE 0)
set(HAVE_COMPRESSION 0)

if(ENABLE_ZLIB)
    find_package(ZLIB)
    if(ZLIB_FOUND)
        set(HAVE_ZLIB 1)
        message(STATUS "zlib support: ENABLED")
    else()
        message(STATUS "zlib support: DISABLED (library not found)")
    endif()
endif()

if(ENABLE_BZIP2)
    find_package(BZip2)
    if(BZIP2_FOUND)
        set(HAVE_BZIP2 1)
        message(STATUS "bzip2 support: ENABLED")
    else()
        message(STATUS "bzip2 support: DISABLED (library not found)")
    endif()
endif()

if(ENABLE_LZMA)
    find_package(LibLZMA)
    if(LIBLZMA_FOUND)
        set(HAVE_LZMA 1)
        message(STATUS "lzma support: ENABLED")
    else()
        message(STATUS "lzma support: DISABLED (library not found)")
    endif()
endif()

if(ENABLE_ZSTD)
    find_package(PkgConfig)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(ZSTD libzstd)
        if(ZSTD_FOUND)
            set(HAVE_ZSTD 1)
            message(STATUS "zstd support: ENABLED")
        else()
            message(STATUS "zstd support: DISABLED (library not found)")
        endif()
    else()
        message(STATUS "zstd support: DISABLED (pkg-config not found)")
    endif()
endif()

if(ENABLE_LIBARCHIVE)
    find_package(LibArchive)
    if(LibArchive_FOUND)
        set(HAVE_LIBARCHIVE 1)
        message(STATUS "libarchive support: ENABLED")
    else()
        message(STATUS "libarchive support: DISABLED (library not found)")
    endif()
endif()

# Set helper variable for any compression library
if(HAVE_ZLIB OR HAVE_BZIP2 OR HAVE_LZMA OR HAVE_ZSTD)
    set(HAVE_COMPRESSION 1)
    message(STATUS "Compression support: ENABLED (at least one compression library available)")
else()
    set(HAVE_COMPRESSION 0)
    message(STATUS "Compression support: DISABLED (no compression libraries available)")
endif()

# Generate config header
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/config.h
)

# Core sources
set(STREAM_SOURCES
    src/stream.c
    src/file_stream.c
    src/mem_stream.c
    src/walker.c
)

# Optional compression support - include if ANY compression library available
if(HAVE_COMPRESSION)
    list(APPEND STREAM_SOURCES src/compression_stream.c)
endif()

# Optional archive support
if(HAVE_LIBARCHIVE)
    list(APPEND STREAM_SOURCES src/archive_stream.c)
endif()

# Create library
add_library(stream ${STREAM_SOURCES})

target_include_directories(stream PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Link optional libraries
if(HAVE_ZLIB)
    target_link_libraries(stream PUBLIC ZLIB::ZLIB)
endif()

if(HAVE_BZIP2)
    target_link_libraries(stream PUBLIC BZip2::BZip2)
endif()

if(HAVE_LZMA)
    target_link_libraries(stream PUBLIC LibLZMA::LibLZMA)
endif()

if(HAVE_ZSTD)
    target_include_directories(stream PUBLIC ${ZSTD_INCLUDE_DIRS})
    target_link_directories(stream PUBLIC ${ZSTD_LIBRARY_DIRS})
    target_link_libraries(stream PUBLIC ${ZSTD_LIBRARIES})
endif()

if(HAVE_LIBARCHIVE)
    target_link_libraries(stream PUBLIC LibArchive::LibArchive)
endif()

# Platform-specific libraries
if(NOT WIN32)
    target_link_libraries(stream PUBLIC m)
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(stream PRIVATE /W4)
else()
    target_compile_options(stream PRIVATE -Wall -Wextra -pedantic)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Installation
install(TARGETS stream
    EXPORT streamio-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/config.h
    DESTINATION include
)
