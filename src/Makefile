SUBDIRS = ptex utils doc # prman tests

.PHONY: $(SUBDIRS)

BUILDUIDIR = ../$(shell uname)-$(shell uname -r | cut -d'-' -f1)-$(shell uname -m)-optimize
INSTALLDIR = ../install

install: $(BUILDUIDIR)
	@for s in $(SUBDIRS); do \
		$(MAKE) -C $$s install; \
	done

# add symlink to platform-specific build dir for buildUI tool
$(BUILDUIDIR):
	ln -sf install $(BUILDUIDIR)

clean:
	@for s in $(SUBDIRS); do \
		$(MAKE) -C $$s clean; \
	done
	rm -rf $(INSTALLDIR)

zip:
	@rm -f ../ptex.zip
	cd .. ; zip ptex.zip install/doc/ptex/* src/{ptex,utils,prman,tests}/*
	@echo "Done regenerating ../ptex.zip"
