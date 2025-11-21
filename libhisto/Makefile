# makefile de biblioteca
CC ?= gcc
AR ?= ar
CFLAGS ?= -O2 -Wall -Wextra -std=c11
INCS := -Iinclude

LIBNAME := libhisto.a
LIBOBJ := src/histo.o

TESTBIN := demo
TESTSRC := tests/demo.c

all: $(LIBNAME) $(TESTBIN)

$(LIBNAME): $(LIBOBJ)
	$(AR) rcs $@ $^

src/%.o: src/%.c include/histo.h include/histo_ioctl.h
	$(CC) $(CFLAGS) $(INCS) -c $< -o $@

$(TESTBIN): $(TESTSRC) $(LIBNAME)
	$(CC) $(CFLAGS) $(INCS) $< -L. -lhisto -o $@

clean:
	rm -f $(LIBOBJ) $(LIBNAME) $(TESTBIN)

.PHONY: all clean