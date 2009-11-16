include ../Makefile.defs

INCPATH  = -I../ptex
CXXFLAGS += $(INCPATH)
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


