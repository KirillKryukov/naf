.POSIX:
.SUFFIXES:

TARGET = unnaf

CC = cc
CFLAGS = -std=gnu11 -Wall -Wextra -flto -O3 -march=native -ffast-math -s
LDFLAGS = -lzstd

.PHONY: default all clean install uninstall

SOURCES = $(wildcard src/*)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ src/unnaf.c $(LDFLAGS)

default: $(TARGET)

all: default

clean:
	-rm -f $(TARGET)

install: $(TARGET)
	mkdir -p $(DESTDIR)$(prefix)/bin
	cp -f $(TARGET) $(DESTDIR)$(prefix)/bin

uninstall:
	-rm -f $(DESTDIR)$(prefix)/bin/$(TARGET)
