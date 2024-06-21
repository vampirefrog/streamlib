CC=clang
AR=ar
RM=rm
CFLAGS=-Wall -Wextra -Werror -O2

ifdef HAVE_LIBZIP
CFLAGS+=-DHAVE_LIBZIP
endif

.PHONY: all clean

all: libstream.a

libstream.a: stream.o
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) -f libstream.a *.o
