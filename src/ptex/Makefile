CC       = gcc
CXX      = g++
# DEBUG = -g -DDEBUG
DEBUG    = -O2 -DNDEBUG
INCPATH  = -I. -I ${RP_subEngine}/include
CXXFLAGS = -Wall -W -pedantic -std=c++98 $(DEBUG) $(INCPATH) -fPIC
LINK     = g++
LFLAGS   = 
LIBS     = -lm -lz -lpthread

OBJECTS = \
	PtexCache.o \
	PtexFilterKernel.o \
	PtexFilters.o \
	PtexHalf.o \
	PtexLockFile.o \
	PtexMitchellFilter.o \
	PtexReader.o \
	PtexSeparableFilter.o \
	PtexSeparableKernel.o \
	PtexUtils.o \
	PtexWriter.o

INSTALLDIR = ../../install

INSTALL = \
	lib/libPtex.a \
	lib/libPtex.so \
	bin/ptxinfo \
	include/Ptexture.h \
	include/PtexHalf.h \
	include/PtexUtils.h

ALL = libPtex.a libPtex.so ptxinfo subdivideobj unsubdivideptx

TESTS = halftest wtest rtest ftest

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
	rm -f *.o $(ALL) $(TESTS) test.ptx

libPtex.a : libPtex.a($(OBJECTS))

libPtex.so: $(OBJECTS)
	$(LINK) $(LFLAGS) -shared -o $@ $(OBJECTS) $(LIBS)

ptxinfo: ptxinfo.o libPtex.a
	$(LINK) $(LFLAGS) $^ -o $@ $(LIBS)

subdivideobj: subdivideobj.o mesh.o
	$(LINK) $(LFLAGS) $^ -o $@ $(LIBS) ${RP_subEngine}/lib/libSubEngine.so

unsubdivideptx: unsubdivideptx.o mesh.o libPtex.a
	$(LINK) $(LFLAGS) $^ -o $@ $(LIBS) ${RP_subEngine}/lib/libSubEngine.so

tests: $(TESTS)

halftest: halftest.o PtexHalf.o
	$(LINK) $(LFLAGS) $^ -o $@

wtest: wtest.o libPtex.a
	$(LINK) $(LFLAGS) $^ -o $@ $(LIBS)

rtest: rtest.o libPtex.a
	$(LINK) $(LFLAGS) $^ -o $@ $(LIBS)

ftest: ftest.o libPtex.a
	$(LINK) $(LFLAGS) $^ -o $@ $(LIBS)


include Makefile.deps
