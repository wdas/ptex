# use compiler from CXX env var if present, otherwise default to g++
ifndef CXX
CXX      = g++
LINK     = g++
else
LINK = $(CXX)
endif

# DEBUG = -g -DDEBUG
DEBUG    = -O2 -DNDEBUG
INCPATH  = -I../../../ptex
CXXFLAGS = -Wall -W -pedantic -std=c++98 $(DEBUG) $(INCPATH) -fPIC
LFLAGS   = 
PTEX     = ../../../ptex/libPtex.a
LIBS     = -lm -lz -lpthread 

all : ptextest.slo mkptx

ptextest.slo : ptextest.sl
	shader -I../.. ptextest.sl

mkptx: mkptx.o $(PTEX)
	$(LINK) $(LFLAGS) $^ -o $@ $(LIBS)

clean:
	rm -f *.o $(ALL)

