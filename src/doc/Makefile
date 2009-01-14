INSTALLDIR = ../../install/doc/html

.PHONY : all install dox

all: install dox

install: $(INSTALLDIR)/PtexFile.html

dox:
	doxygen Doxyfile

$(INSTALLDIR)/PtexFile.html : PtexFile.html
	@mkdir -p $(INSTALLDIR)
	cp $^ $@
