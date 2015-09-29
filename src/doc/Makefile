
.PHONY : all doc

all: doc

INSTALLDIR = ../../install/share/doc/ptex

doc:
	mkdir -p $(INSTALLDIR)
	doxygen Doxyfile
	rsync -vrP ptex/ $(INSTALLDIR)/
	install -m 644 *.png $(INSTALLDIR)

clean:
	rm -rf $(INSTALLDIR)
