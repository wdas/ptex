CC       = gcc
CXX      = g++
# DEBUG = -g -DDEBUG
DEBUG    = -O2 -DNDEBUG
INCPATH  = -I../ptex
CXXFLAGS = -Wall -W -pedantic -std=c++98 $(DEBUG) $(INCPATH) -fPIC
LINK     = g++
LFLAGS   = 
PTEX     = ../ptex/libPtex.a
LIBS     = -lm -lz -lpthread 

TESTS = halftest wtest rtest ftest

tests: $(TESTS)

clean:
	rm -f *.o $(ALL)

halftest: halftest.o $(PTEX)
	$(LINK) $(LFLAGS) $^ -o $@ $(LIBS)

wtest: wtest.o $(PTEX)
	$(LINK) $(LFLAGS) $^ -o $@ $(LIBS)

rtest: rtest.o $(PTEX)
	$(LINK) $(LFLAGS) $^ -o $@ $(LIBS)

ftest: ftest.o $(PTEX)
	$(LINK) $(LFLAGS) $^ -o $@ $(LIBS)


