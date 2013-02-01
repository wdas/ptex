include ../etc/common.mak

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
	include/PtexHalf.h \
	include/PtexInt.h \
	include/PtexUtils.h

ALL = libPtex.a 

ifndef PTEX_STATIC
ALL += libPtex.$(DSOEXT)
INSTALL += $(LIBDIR)/libPtex.$(DSOEXT)
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


ifeq ($(SYSTEM),Darwin)
STATIC_DEPS = $(OBJECTS)
STATIC_ARCHIVE = libtool -o $@ -static $^
else
STATIC_DEPS = libPtex.a($(OBJECTS))
STATIC_ARCHIVE =
endif

ifdef PTEX_STATIC
libPtex.a : $(STATIC_DEPS)
	$(STATIC_ARCHIVE)

else
libPtex.a : $(STATIC_DEPS)
	$(STATIC_ARCHIVE)

libPtex.$(DSOEXT): $(OBJECTS)
	$(LINK) $(LFLAGS) -shared -o $@ $(OBJECTS) $(LIBS)
endif

depend:
	$(CXX) -MM $(SRCS) > Makefile.deps

include Makefile.deps
