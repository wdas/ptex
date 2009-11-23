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
PTEX     = ../ptex/libPtex.a
LIBS     = -lm -lz -lpthread 

TESTS = halftest wtest rtest ftest

tests: $(TESTS)
	./run_tests.py

clean:
	rm -f $(ALL)

halftest: halftest.cpp $(PTEX)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)

wtest: wtest.cpp $(PTEX)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)

rtest: rtest.cpp $(PTEX)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)

ftest: ftest.cpp $(PTEX)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)


