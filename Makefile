.POSIX:
.SUFFIXES:

export prefix = /usr/local

.PHONY: default all clean install uninstall

default:
	$(MAKE) -C zstd/lib libzstd.a
	$(MAKE) -C ennaf
	$(MAKE) -C unnaf

all: default

clean:
	$(MAKE) -C ennaf clean
	$(MAKE) -C unnaf clean

install:
	$(MAKE) -C ennaf install
	$(MAKE) -C unnaf install

uninstall:
	$(MAKE) -C ennaf uninstall
	$(MAKE) -C unnaf uninstall
