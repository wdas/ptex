CC       = gcc
CXX      = g++
# DEBUG = -g -DDEBUG
DEBUG    = -O2 -DNDEBUG
INCPATH  = -I../ptex -I ${RP_subEngine}/include -I${RMANTREE}/include
CXXFLAGS = -Wall -W -pedantic -std=c++98 $(DEBUG) $(INCPATH) -fPIC
LINK     = g++
LFLAGS   = 
PTEX     = ../ptex/libPtex.a
LIBS     = -lm -lz -lpthread ${RP_subEngine}/lib/libSubEngine.so

INSTALLDIR = ../../install

INSTALL = \
	bin/ptxinfo \
	bin/subdivideobj \
	bin/unsubdivideptx \
	bin/objtorib

ALL = ptxinfo subdivideobj unsubdivideptx objtorib

$(INSTALLDIR)/bin/% : %
	@ mkdir -p $(@D)
	cp $^ $@

install: all $(patsubst %,$(INSTALLDIR)/%,$(INSTALL))

all: $(ALL)

clean:
	rm -f *.o $(ALL)

ptxinfo: ptxinfo.o $(PTEX)
	$(LINK) $(LFLAGS) $^ -o $@ $(LIBS)

subdivideobj: subdivideobj.o mesh.o
	$(LINK) $(LFLAGS) $^ -o $@ $(LIBS) 

objtorib: objtorib.o mesh.o ${RMANTREE}/lib/libprman.so
	$(LINK) $(LFLAGS) $^ -o $@ $(LIBS) 

unsubdivideptx: unsubdivideptx.o mesh.o $(PTEX)
	$(LINK) $(LFLAGS) $^ -o $@ $(LIBS)

