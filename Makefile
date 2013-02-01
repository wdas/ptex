#!/usr/bin/make
prefix ?= $(shell pf-makevar --absolute root)
libdir ?= $(shell pf-makevar lib)

.PHONY: install clean

install:
	$(MAKE) -C src INSTALLDIR=$(DESTDIR)$(prefix) LIBDIR=$(libdir) install

clean:
	$(MAKE) -C src INSTALLDIR=$(DESTDIR)$(prefix) LIBDIR=$(libdir) clean
