CC=gcc
AR=ar
RM=rm
CFLAGS=-Wall -Wextra -Werror -O2
LDFLAGS=

ifdef HAVE_LIBZIP
CFLAGS+=-DHAVE_LIBZIP $(shell pkg-config --cflags libzip)
LDFLAGS+=$(shell pkg-config --libs libzip)
endif

ifdef HAVE_GZIP
CFLAGS+=-DHAVE_GZIP $(shell pkg-config --cflags zlib)
LDFLAGS+=$(shell pkg-config --libs zlib)
endif

.PHONY: all tests clean

all: libstream.a

libstream.a: stream_base.o file_stream.o mem_stream.o zip_file_stream.o each_file.o
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

TESTS = \
	tests/test_zip_file_stream_mmap \
	tests/test_zip_file_stream_gz_mmap

tests: libstream.a
	cd tests && $(CC) each_file.c ../libstream.a $(CFLAGS) $(LDFLAGS) -o each_file && ./each_file
	cd tests && $(CC) stream.c ../libstream.a $(CFLAGS) $(LDFLAGS) -o stream && ./stream
	cd tests && $(CC) test_zip_file_stream_mmap.c ../libstream.a $(CFLAGS) $(LDFLAGS) -o test_zip_file_stream_mmap && ./test_zip_file_stream_mmap
	cd tests && $(CC) test_zip_file_stream_gz_mmap.c ../libstream.a $(CFLAGS) $(LDFLAGS) -o test_zip_file_stream_gz_mmap && ./test_zip_file_stream_gz_mmap

clean:
	$(RM) -f libstream.a *.o tests/*.exe tests/each_file tests/stream tests/test_zip_file_stream_mmap tests/test_zip_file_stream_gz_mmap
