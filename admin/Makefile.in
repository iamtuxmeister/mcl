# This will not build but on my machine
BASEDIR=/u/erwin/ed/
VERSION := `tail -1 $(BASEDIR)/mcl/VERSION`
MCL := mcl-$(VERSION)
DOCS := doc/README doc/Embedded doc/Modules doc/Examples doc/Chat
HTMLDOCS := $(patsubst %, %.html, $(DOCS))

ALL_SOURCE = $(SRC_FILES) $(INC_FILES)
# Common files for all distributions
_COMMON_FILES := $(DOCS) samples VERSION doc/*.html doc/Changes doc/TODO INSTALL README.1ST doc/Plugins
_SOURCE_FILES := $(ALL_SOURCE) *.c $(INCL) Makefile.in admin/Makefile.in admin/config.* configure.in configure \
.depend o/dummy plugins/*.cc o/plugins/.dummy doc/mcl.spec admin/install-sh \
doc/mcl.spec.in VERSION.in h/config.h.in

_BINARY_FILES := mcl plugins/*.so

COMMON_FILES := $(patsubst %, $(MCL)/%, $(_COMMON_FILES))
SOURCE_FILES := $(patsubst %, $(MCL)/%, $(_SOURCE_FILES))
BINARY_FILES := $(patsubst %, $(MCL)/%, $(_BINARY_FILES))

ftp:	distrib
	./put

%.html: %
	util/htmlize.pl $<

docs: $(HTMLDOCS)

EXCLUDE := --exclude CVS --exclude localinit.pl --exclude config --exclude mclrc --exclude logs \
	--exclude config.perl --exclude config.python --exclude history

rpms: docs
	set +x; \
	cd /usr/src/redhat/SPECS ; \
	cp mcl.spec mcl-$(VERSION).spec ; \
	perl -pi -e "s/^Version.*/Version: $(VERSION)/" mcl-$(VERSION).spec ; \
	cp $(BASEDIR)/mcl/dist/mcl-$(VERSION)-src.tar.gz /usr/src/redhat/SOURCES ; \
	rpm -ba mcl-$(VERSION).spec ; \
	cd /usr/src/redhat ; \
	cp RPMS/*/mcl-$(VERSION)*rpm $(BASEDIR)/mcl/dist; \
	cp SRPMS/mcl-$(VERSION)*rpm $(BASEDIR)/mcl/dist ; \
	rm SPECS/mcl-$(VERSION).spec

autoconf:
	autoconf
	CXXFLAGS=-O ./configure --enable-debug


srcdistrib:
	rm -f dist/*.gz
	ln -s $(BASEDIR)/mcl ../$(MCL)
	strip mcl o/plugins/*so
	ln -sf o/plugins/*so plugins/
	(cd .. ; tar cvfh $(MCL)/dist/mcl-$(VERSION)-src.tar $(EXCLUDE) $(COMMON_FILES) $(SOURCE_FILES))
	(cd .. ; tar cvfh $(MCL)/dist/mcl-$(VERSION)-bin.tar $(EXCLUDE) $(COMMON_FILES) $(BINARY_FILES))
	gzip -c9 dist/mcl-$(VERSION)-src.tar >  dist/mcl-$(VERSION)-src.tar.gz
	gzip -c9 dist/mcl-$(VERSION)-bin.tar >  dist/mcl-$(VERSION)-bin.tar.gz
	rm ../$(MCL)
	rm dist/*tar

distrib:
	find samples -name "*~" -print | xargs rm -f
	$(MAKE) autoconf
	$(MAKE) docs
#	$(MAKE) commit
	$(MAKE) all
	$(MAKE) srcdistrib
#	bzip2 -c9 dist/mcl-$(VERSION)-src.tar >  dist/mcl-$(VERSION)-src.tar.bz2
#	bzip2 -c9 dist/mcl-$(VERSION)-bin.tar >  dist/mcl-$(VERSION)-bin.tar.bz2
	$(MAKE) rpms
	touch ~/html/mcl/index.sl 			# Make sure it gets rebuilt
	util/asksync.sh

commit:
	util/checkversion.sh
	util/docommit.sh
