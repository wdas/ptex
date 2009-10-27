include ../Makefile.defs

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
