.POSIX:
.SUFFIXES:

export prefix = /usr/local

.PHONY: default all clean install uninstall

default:
	$(MAKE) -C zstd/lib ZSTD_LEGACY_SUPPORT=0 ZSTD_LIB_DEPRECATED=0 ZSTD_LIB_DICTBUILDER=0 libzstd.a
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
