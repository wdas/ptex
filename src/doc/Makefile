
.PHONY : all doc

all: doc

INSTALLDIR = ../../install/doc/ptex

doc:
	mkdir -p $(INSTALLDIR)
	doxygen Doxyfile

clean:
	rm -rf $(INSTALLDIR)
