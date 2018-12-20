.POSIX:
.SUFFIXES:

.PHONY: default all clean install uninstall

default: ennaf.pl

all: default

clean:

install: ennaf.pl
	mkdir -p $(DESTDIR)$(prefix)/bin
	cp -f ennaf.pl $(DESTDIR)$(prefix)/bin
	ln -sf $(DESTDIR)$(prefix)/bin/ennaf.pl $(DESTDIR)$(prefix)/bin/ennaf

uninstall:
	-rm -f $(DESTDIR)$(prefix)/bin/ennaf
	-rm -f $(DESTDIR)$(prefix)/bin/ennaf.pl
