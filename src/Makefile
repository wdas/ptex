SUBDIRS = ptex # utils prman tests

.PHONY: $(SUBDIRS)

install: $(SUBDIRS)

clean:
	for s in $(SUBDIRS); do \
		$(MAKE) -C $$s clean; \
	done

$(SUBDIRS):
	$(MAKE) -C $@

zip:
	@rm -f ../ptex.zip
	cd .. ; zip ptex.zip install/doc/html/* src/{ptex,utils,prman,tests}/*
	@echo "Done regenerating ../ptex.zip"
