CC=gcc
AR=ar
RM=rm
CFLAGS=-Wall -Wextra -Werror -O2
LDFLAGS=

ifdef HAVE_LIBZIP
CFLAGS+=-DHAVE_LIBZIP
LDFLAGS+=-lzip
endif

.PHONY: all tests clean

all: libstream.a

libstream.a: stream.o
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

tests: libstream.a
	cd tests && $(CC) each_file.c ../libstream.a $(LDFLAGS) -o each_file && ./each_file
	cd tests && $(CC) stream.c ../libstream.a $(LDFLAGS) -o stream && ./stream

clean:
	$(RM) -f libstream.a *.o tests/*.exe tests/each_file tests/stream
