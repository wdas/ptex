PTEXDIR  = ../../install
CC       = gcc
CXX      = g++
# DEBUG = -g -DDEBUG
DEBUG    = -O2 -DNDEBUG
INCPATH  = -I. -I$(PTEXDIR)/include -I$(RMANTREE)/include
#CXXFLAGS = -Wall -W -pedantic -std=c++98 $(DEBUG) $(INCPATH) -fPIC
CXXFLAGS = -Wall -W $(DEBUG) $(INCPATH) -fPIC
LINK     = g++
LFLAGS   = 
LIBS     = -lm -lz -lpthread $(PTEXDIR)/lib/libPtex.a

ALL = ptextureShadeop.so

all: $(ALL)


clean:
	rm -f *.o $(ALL)

ptextureShadeop.so: ptextureShadeop.o
	$(LINK) $(LFLAGS) -shared -o $@ $^ $(LIBS)
