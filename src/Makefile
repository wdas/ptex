SUBDIRS = ptex utils tests

.PHONY: subdirs $(SUBDIRS)

subdirs: $(SUBDIRS)

clean:
	for s in $(SUBDIRS); do \
		$(MAKE) -C $$s clean; \
	done

$(SUBDIRS):
	$(MAKE) -C $@
