CC=gcc
AR=ar
RM=rm
CFLAGS=-Wall -Wextra -Werror -O2
LDFLAGS=

ifdef HAVE_LIBZIP
CFLAGS+=-DHAVE_LIBZIP $(pkg-config --cflags libzip)
LDFLAGS+=$(shell pkg-config --libs libzip)
endif

ifdef HAVE_GZIP
CFLAGS+=-DHAVE_GZIP $(pkg-config --cflags zlib)
LDFLAGS+=$(shell pkg-config --libs zlib)
endif

.PHONY: all tests clean

all: libstream.a

libstream.a: each_file.o file_stream.o mem_stream.o stream.o zip_file_stream.o
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

tests: libstream.a
	cd tests && $(CC) each_file.c ../libstream.a $(CFLAGS) $(LDFLAGS) -o each_file && ./each_file
	cd tests && $(CC) stream.c ../libstream.a $(CFLAGS) $(LDFLAGS) -o stream && ./stream

clean:
	$(RM) -f libstream.a *.o tests/*.exe tests/each_file tests/stream
