.POSIX:
.SUFFIXES:

TARGET = unnaf

CC = cc
CFLAGS = -std=gnu99 -Wall -Wextra -O3 -march=native -ffast-math -s
LDFLAGS ?= -l:libzstd.a

.PHONY: default all clean install uninstall

SOURCES = $(wildcard src/*)

default: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ src/unnaf.c $(LDFLAGS)

all: default

clean:
	-rm -f $(TARGET)

install: $(TARGET)
	mkdir -p $(DESTDIR)$(prefix)/bin
	cp -f $(TARGET) $(DESTDIR)$(prefix)/bin

uninstall:
	-rm -f $(DESTDIR)$(prefix)/bin/$(TARGET)
