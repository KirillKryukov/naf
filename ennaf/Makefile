.POSIX:
.SUFFIXES:

TARGET = ennaf

CC ?= gcc
ifeq ($(origin CC),default)
CC = gcc
endif

CFLAGS = -std=gnu99 -Wall -Wextra -O3 -march=native -ffast-math -s -I../zstd/lib
LDFLAGS = ../zstd/lib/libzstd.a

.PHONY: default all clean install uninstall

SOURCES = $(wildcard src/*)

default: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ src/ennaf.c $(LDFLAGS)

all: default

clean:
	-rm -f $(TARGET)

install: $(TARGET)
	mkdir -p $(DESTDIR)$(prefix)/bin
	cp -f $(TARGET) $(DESTDIR)$(prefix)/bin

uninstall:
	-rm -f $(DESTDIR)$(prefix)/bin/$(TARGET)
