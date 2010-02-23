# use compiler from CXX env var if present, otherwise default to g++
ifndef CXX
CXX      = g++
LINK     = g++
else
LINK = $(CXX)
endif

# DEBUG = -g -DDEBUG
DEBUG    = -O2 -DNDEBUG
INCPATH  = -I../ptex
CXXFLAGS = -Wall -W -pedantic -std=c++98 $(DEBUG) $(INCPATH) -fPIC
PTEX     = ../ptex/libPtex.a
LIBS     = -lm -lz -lpthread

ifdef PTEX_STATIC
CXXFLAGS += -DPTEX_STATIC
endif


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

PTEX_SHA = $(shell git rev-list --max-count=1 HEAD)
PTEX_VER = $(shell git describe $(PTEX_SHA)) ($(PTEX_SHA))

ptxinfo: ptxinfo.cpp $(PTEX)
	$(CXX) $(CXXFLAGS) -DPTEX_VER='"$(PTEX_VER)"' $^ -o $@ $(LIBS)

