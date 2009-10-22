# use compiler from CXX env var if present, otherwise default to g++
ifndef CXX
CXX      = g++
LINK     = g++
else
LINK = $(CXX)
endif

# DEBUG = -g -DDEBUG
DEBUG    = -O2 -DNDEBUG
INCPATH  = -I../ptex -I ${RP_subEngine}/include -I${RMANTREE}/include
# prman headers won't compile with -pedantic
#CXXFLAGS = -Wall -W -pedantic -std=c++98 $(DEBUG) $(INCPATH) -fPIC
CXXFLAGS = -Wall -W -std=c++98 $(DEBUG) $(INCPATH) -fPIC
PTEX     = ../ptex/libPtex.a
LIBS     = -lm -lz -lpthread ${RP_subEngine}/lib/libSubEngine.so

INSTALLDIR = ../../install

INSTALL = \
	bin/ptxinfo \
	bin/subdivideobj \
	bin/subdivideptx \
	bin/unsubdivideptx \
	bin/objtorib

ALL = ptxinfo subdivideobj subdivideptx unsubdivideptx objtorib

$(INSTALLDIR)/bin/% : %
	@ mkdir -p $(@D)
	cp $^ $@

install: all $(patsubst %,$(INSTALLDIR)/%,$(INSTALL))

all: $(ALL)

clean:
	rm -f $(ALL)

ptxinfo: ptxinfo.cpp $(PTEX)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)

subdivideobj: subdivideobj.cpp mesh.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)

objtorib: objtorib.cpp mesh.cpp ${RMANTREE}/lib/libprman.so
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)

subdivideptx: subdivideptx.cpp mesh.cpp $(PTEX)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)

unsubdivideptx: unsubdivideptx.cpp mesh.cpp $(PTEX)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)

