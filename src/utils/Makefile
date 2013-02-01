include ../etc/common.mak

PTEX     = ../ptex/libPtex.a

INSTALL = \
	bin/ptxinfo

INSTALLDIR = ../../install
INSTALLPATHS = $(patsubst %,$(INSTALLDIR)/%,$(INSTALL))


ALL = ptxinfo

$(INSTALLDIR)/bin/% : %
	@ mkdir -p $(@D)
	cp $^ $@

install: $(INSTALLPATHS)

all: install $(ALL)

clean:
	rm -f $(ALL) $(INSTALLPATHS)

PTEX_SHA = $(shell git rev-parse HEAD)
PTEX_VER = $(shell git describe $(PTEX_SHA)) ($(PTEX_SHA))

ptxinfo: ptxinfo.cpp $(PTEX)
	$(CXX) $(CXXFLAGS) -DPTEX_VER='"$(PTEX_VER)"' $^ -o $@ $(LIBS)

