# use compiler from CXX env var if present, otherwise default to g++
ifndef CXX
CXX      = g++
LINK     = g++
else
LINK = $(CXX)
endif

# DEBUG = -g -DDEBUG
DEBUG    = -O3 -DNDEBUG -msse4.1
INCPATH  = -I.
DEFINES  = 

ifdef PRMAN_15_COMPATIBLE_PTEX
DEFINES += -DPTEX_NO_LARGE_METADATA_BLOCKS
endif

ifdef PTEX_STATIC
DEFINES += -DPTEX_STATIC
endif

CXXFLAGS = -Wall -Wconversion -pedantic -W -std=c++98 $(DEBUG) $(INCPATH) $(DEFINES) -fPIC
LFLAGS   = 
LIBS     = -lm -lz -lpthread

-include ../config.mak

SRCS = \
	PtexCache.cpp \
	PtexFilters.cpp \
	PtexHalf.cpp \
	PtexReader.cpp \
	PtexSeparableFilter.cpp \
	PtexSeparableKernel.cpp \
	PtexTriangleFilter.cpp \
	PtexTriangleKernel.cpp \
	PtexUtils.cpp \
	PtexWriter.cpp

OBJECTS = $(patsubst %.cpp,%.o,$(SRCS))

INSTALLDIR = ../../install
LIBDIR = lib

INSTALL = \
	$(LIBDIR)/libPtex.a \
	include/Ptexture.h \
	include/PtexVersion.h \
	include/PtexHalf.h \
	include/PtexInt.h \
	include/PtexUtils.h

ALL = libPtex.a 

ifndef PTEX_STATIC
ALL += libPtex.so
INSTALL += $(LIBDIR)/libPtex.so
endif

$(INSTALLDIR)/$(LIBDIR)/% : %
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

ifdef PTEX_STATIC
libPtex.a : libPtex.a($(OBJECTS))
	ar -r $@

else
libPtex.a : libPtex.a($(OBJECTS))

libPtex.so: $(OBJECTS)
	$(LINK) $(LFLAGS) -shared -o $@ $(OBJECTS) $(LIBS)
endif

depend:
	$(CXX) -MM $(SRCS) > Makefile.deps

include Makefile.deps
