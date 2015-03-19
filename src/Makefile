SUBDIRS = ptex utils tests
INSTALLDIR = ../../install
LIBDIR = lib

.PHONY: subdirs install clean $(SUBDIRS)

subdirs: $(SUBDIRS)

install:
	for s in $(SUBDIRS); do \
		$(MAKE) -C $$s INSTALLDIR=$(INSTALLDIR) LIBDIR=$(LIBDIR) install; \
	done

clean:
	for s in $(SUBDIRS); do \
		$(MAKE) -C $$s clean; \
	done

$(SUBDIRS):
	$(MAKE) -C $@ INSTALLDIR=$(INSTALLDIR) LIBDIR=$(LIBDIR)
