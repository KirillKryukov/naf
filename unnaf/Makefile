
TARGET = unnaf

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O3 -ffast-math -s

.PHONY: default all clean

default: $(TARGET)
all: default

SOURCES = $(wildcard src/*) 

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ src/unnaf.c -lzstd

clean:
	-rm -f $(TARGET)
