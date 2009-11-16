include ../Makefile.defs

INCPATH  = -I../ptex -I${RMANTREE}/include
INCSUBD  = -I${RP_subEngine}/include 
CXXFLAGS += $(INCPATH)

PTEX     = ../ptex/libPtex.a
LIBS     = -lm -lz -lpthread
LIBSUBD  = ${RP_subEngine}/lib/libSubEngine.so

INSTALL = \
	bin/ptxinfo
#	bin/subdivideobj \
#	bin/subdivideptx \
#	bin/unsubdivideptx \
#	bin/objtorib

INSTALLDIR = ../../install
INSTALLPATHS = $(patsubst %,$(INSTALLDIR)/%,$(INSTALL))

ALL = ptxinfo subdivideobj subdivideptx unsubdivideptx objtorib

$(INSTALLDIR)/bin/% : %
	@ mkdir -p $(@D)
	cp $^ $@

install: $(INSTALLPATHS)

all: install $(ALL)

clean:
	rm -f $(ALL) $(INSTALLPATHS)

PTEX_SHA = $(shell git rev-list --max-count=1 HEAD)
PTEX_VER = $(shell git describe $(PTEX_SHA)) ($(PTEX_SHA))

ptxinfo: ptxinfo.cpp $(PTEX)
	$(CXX) $(CXXFLAGS) -DPTEX_VER='"$(PTEX_VER)"' $^ -o $@ $(LIBS)

subdivideobj: subdivideobj.cpp mesh.cpp
	$(CXX) $(CXXFLAGS) $(INCSUBD) $^ -o $@ $(LIBS) $(LIBSUBD)

objtorib: objtorib.cpp mesh.cpp ${RMANTREE}/lib/libprman.so
	$(CXX) $(CXXFLAGS) $(INCSUBD) $^ -o $@ $(LIBS) $(LIBSUBD)

subdivideptx: subdivideptx.cpp mesh.cpp $(PTEX)
	$(CXX) $(CXXFLAGS) $(INCSUBD) $^ -o $@ $(LIBS) $(LIBSUBD)

unsubdivideptx: unsubdivideptx.cpp mesh.cpp $(PTEX)
	$(CXX) $(CXXFLAGS) $(INCSUBD) $^ -o $@ $(LIBS) $(LIBSUBD)

