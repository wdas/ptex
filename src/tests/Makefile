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


