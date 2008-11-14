CC       = gcc
CXX      = g++
# DEBUG = -g -DDEBUG
DEBUG    = -O2 -DNDEBUG
INCPATH  = -I.
CXXFLAGS = -Wall -W -pedantic -std=c++98 $(DEBUG) $(INCPATH) -fPIC
LINK     = g++
LFLAGS   = 
LIBS     = -lm -lz -lpthread

OBJECTS = \
	PtexCache.o \
	PtexFilterKernel.o \
	PtexHalf.o \
	PtexLockFile.o \
	PtexMitchellFilter.o \
	PtexReader.o \
	PtexUtils.o \
	PtexWriter.o


all: libPtex.so ptxinfo

libPtex.so: $(OBJECTS)
	$(LINK) $(LFLAGS) -shared -o $@ $(OBJECTS) $(LIBS)

ptxinfo: ptxinfo.o libPtex.so
	$(LINK) $(LFLAGS) $^ -o $@

tests: halftest wtest rtest ftest libPtex.so

halftest: halftest.o PtexHalf.o
	$(LINK) $(LFLAGS) $^ -o $@

wtest: wtest.o libPtex.so
	$(LINK) $(LFLAGS) $^ -o $@ 

rtest: rtest.o libPtex.so
	$(LINK) $(LFLAGS) $^ -o $@

ftest: ftest.o libPtex.so
	$(LINK) $(LFLAGS) $^ -o $@


