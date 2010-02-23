# use compiler from CXX env var if present, otherwise default to g++
ifndef CXX
CXX      = g++
LINK     = g++
else
LINK = $(CXX)
endif

# DEBUG = -g -DDEBUG
DEBUG    = -O2 -DNDEBUG
INCPATH  = -I.
DEFINES  = 

ifdef PTEX_STATIC
DEFINES += -DPTEX_STATIC
endif

CXXFLAGS = -Wall -pedantic -W -std=c++98 $(DEBUG) $(INCPATH) $(DEFINES) -fPIC
LFLAGS   = 
LIBS     = -lm -lz -lpthread

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

INSTALL = \
	lib/libPtex.a \
	include/Ptexture.h \
	include/PtexHalf.h \
	include/PtexInt.h \
	include/PtexUtils.h

ALL = libPtex.a 

ifndef PTEX_STATIC
ALL += libPtex.so
INSTALL += lib/libPtex.so
endif

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
