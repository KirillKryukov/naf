.POSIX:
.SUFFIXES:

export prefix = /usr/local

.PHONY: default all test test-large clean install uninstall

default:
	$(MAKE) -C zstd/lib ZSTD_LEGACY_SUPPORT=0 ZSTD_LIB_DEPRECATED=0 ZSTD_LIB_DICTBUILDER=0 libzstd.a
	$(MAKE) -C ennaf
	$(MAKE) -C unnaf

all: default

test:
	$(MAKE) -C tests

test-large:
	$(MAKE) -C tests large

clean:
	$(MAKE) -C ennaf clean
	$(MAKE) -C unnaf clean
	$(MAKE) -C tests clean

install:
	$(MAKE) -C ennaf install
	$(MAKE) -C unnaf install

uninstall:
	$(MAKE) -C ennaf uninstall
	$(MAKE) -C unnaf uninstall
