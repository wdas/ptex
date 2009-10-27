SUBDIRS = ptex utils # prman tests

.PHONY: $(SUBDIRS)

install:
	@for s in $(SUBDIRS); do \
		$(MAKE) -C $$s install; \
	done

clean:
	@for s in $(SUBDIRS); do \
		$(MAKE) -C $$s clean; \
	done

zip:
	@rm -f ../ptex.zip
	cd .. ; zip ptex.zip install/doc/html/* src/{ptex,utils,prman,tests}/*
	@echo "Done regenerating ../ptex.zip"
