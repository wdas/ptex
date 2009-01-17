
.PHONY : all install dox

all: install dox

INSTALLDIR = ../../install/doc/html

INSTALL = \
	PtexFile.html \
	ptex-teaser.png \
	adjdata1.jpg

$(INSTALLDIR)/% : %
	@ mkdir -p $(@D)
	cp $^ $@

install: $(patsubst %,$(INSTALLDIR)/%,$(INSTALL))

dox:
	doxygen Doxyfile
