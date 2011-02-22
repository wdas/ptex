#!/usr/bin/make
prefix ?= $(shell pf-makevar --absolute root)
libdir ?= $(shell pf-makevar lib)

.PHONY: install clean

install:
	$(MAKE) -C src INSTALLDIR=$(DESTDIR)$(prefix) LIBDIR=$(libdir) install
	@/bin/ls -1 $(DESTDIR)$(prefix) > .release.Ptex

clean:
	$(MAKE) -C src INSTALLDIR=$(DESTDIR)$(prefix) LIBDIR=$(libdir) clean
