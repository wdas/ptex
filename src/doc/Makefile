include ../Makefile.defs

.PHONY : install dox

INSTALLDIR = ../../install/doc/ptex

INSTALL = \
	PtexFile.html \
	FilterFootprint.html \
	ptex-teaser.png \
	adjdata1.jpg \
	uvellipse.png

$(INSTALLDIR)/% : %
	@ mkdir -p $(@D)
	cp $^ $@

install: $(patsubst %,$(INSTALLDIR)/%,$(INSTALL)) $(INSTALLDIR)/index.html

$(INSTALLDIR)/index.html : ../ptex/Ptexture.h
	doxygen Doxyfile

clean:
	rm -rf $(INSTALLDIR)
