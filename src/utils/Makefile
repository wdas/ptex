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

INSTALLDIR = ../../install

INSTALL = \
	bin/ptxinfo

ALL = ptxinfo

$(INSTALLDIR)/bin/% : %
	@ mkdir -p $(@D)
	cp $^ $@

install: all $(patsubst %,$(INSTALLDIR)/%,$(INSTALL))

all: $(ALL)

clean:
	rm -f $(ALL)

ptxinfo: ptxinfo.cpp $(PTEX)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)

