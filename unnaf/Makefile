.POSIX:
.SUFFIXES:

TARGET = unnaf

CC = cc
CFLAGS = -std=c99 -Wall -Wextra -O3 -ffast-math -s
LDFLAGS = -lzstd
PREFIX = /usr/local

.PHONY: default all clean install

default: $(TARGET)
all: default

install: $(TARGET)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f $(TARGET) $(DESTDIR)$(PREFIX)/bin

SOURCES = $(wildcard src/*) 

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ src/unnaf.c $(LDFLAGS)

clean:
	-rm -f $(TARGET)
