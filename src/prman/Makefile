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
LIBPTEX = $(PTEXDIR)/lib/libPtex.a
LIBS     = -lm -lz -lpthread

ALL = ptextureShadeop.so

all: $(ALL)


clean:
	rm -f *.o $(ALL)

ptextureShadeop.so: ptextureShadeop.o $(LIBPTEX)
	$(LINK) $(LFLAGS) -shared -o $@ $^ $(LIBS)
