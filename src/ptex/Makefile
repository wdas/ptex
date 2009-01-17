CC       = gcc
CXX      = g++
# DEBUG = -g -DDEBUG
DEBUG    = -O2 -DNDEBUG
INCPATH  = -I.
CXXFLAGS = -Wall -W -pedantic -std=c++98 $(DEBUG) $(INCPATH) -fPIC
LINK     = g++
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
	PtexTrilinearFilter.cpp \
	PtexUtils.cpp \
	PtexWriter.cpp

OBJECTS = $(patsubst %.cpp,%.o,$(SRCS))

INSTALLDIR = ../../install

INSTALL = \
	lib/libPtex.a \
	lib/libPtex.so \
	include/Ptexture.h \
	include/PtexHalf.h \
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

install: all $(patsubst %,$(INSTALLDIR)/%,$(INSTALL))

all: $(ALL)

clean:
	rm -f *.o $(ALL)

libPtex.a : libPtex.a($(OBJECTS))

libPtex.so: $(OBJECTS)
	$(LINK) $(LFLAGS) -shared -o $@ $(OBJECTS) $(LIBS)

depend:
	$(CXX) -MM $(SRCS) > Makefile.deps

include Makefile.deps
