SUBDIRS = ptex utils tests
INSTALLDIR = ../../install
LIBDIR = lib

.PHONY: subdirs $(SUBDIRS)

subdirs: $(SUBDIRS)

clean:
	for s in $(SUBDIRS); do \
		$(MAKE) -C $$s clean; \
	done

$(SUBDIRS):
	$(MAKE) -C $@ INSTALLDIR=$(INSTALLDIR) LIBDIR=$(LIBDIR)
