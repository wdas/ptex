# use compiler from CXX env var if present, otherwise default to g++
ifndef CXX
CXX      = g++
LINK     = g++
else
LINK = $(CXX)
endif

#DEBUG = -g -DDEBUG
DEBUG = -O2 -DNDEBUG

INCPATH  = -I.
CXXFLAGS = -Wall -W -pedantic -std=c++98 $(DEBUG) $(INCPATH) -fPIC
LFLAGS   = 
LIBS     = -lm -lz -lpthread

SRCS = \
	PtexCache.cpp \
	PtexFilterKernel.cpp \
	PtexFilters.cpp \
	PtexHalf.cpp \
	PtexMitchellFilter.cpp \
	PtexReader.cpp \
	PtexSeparableFilter.cpp \
	PtexSeparableKernel.cpp \
	PtexTriangleFilter.cpp \
	PtexTriangleKernel.cpp \
	PtexUtils.cpp \
	PtexWriter.cpp

OBJECTS = $(patsubst %.cpp,%.o,$(SRCS))

INSTALLDIR ?= ../../$(shell uname)-$(shell uname -r | cut -d'-' -f1)-$(shell uname -m)-optimize

INSTALL = \
	lib/libPtex.a \
	lib/libPtex.so \
	include/Ptexture.h \
	include/PtexHalf.h \
	include/PtexInt.h \
	include/PtexUtils.h

ALL = libPtex.a libPtex.so

$(INSTALLDIR)/lib/% : %
	@ mkdir -p $(@D)
	cp $^ $@

$(INSTALLDIR)/bin/% : %
	@ mkdir -p $(@D)
	cp $^ $@

$(INSTALLDIR)/include/% : %
	@ mkdir -p $(@D)
	cp $^ $@

ALLINSTALL = $(patsubst %,$(INSTALLDIR)/%,$(INSTALL))

install: all $(ALLINSTALL)

all: $(ALL)

clean:
	rm -f *.o $(ALL) $(ALLINSTALL)

cleanall: clean

libPtex.a : libPtex.a($(OBJECTS))

libPtex.so: $(OBJECTS)
	$(LINK) $(LFLAGS) -shared -o $@ $(OBJECTS) $(LIBS)

depend:
	$(CXX) -MM $(SRCS) > Makefile.deps

include Makefile.deps
