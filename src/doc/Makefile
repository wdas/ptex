
.PHONY : all install dox

all: install dox

INSTALLDIR = ../../install/doc/html

INSTALL = \
	PtexFile.html \
	FilterFootprint.html \
	ptex-teaser.png \
	adjdata1.jpg \
	uvellipse.png

$(INSTALLDIR)/% : %
	@ mkdir -p $(@D)
	cp $^ $@

install: $(patsubst %,$(INSTALLDIR)/%,$(INSTALL))

dox:
	doxygen Doxyfile
