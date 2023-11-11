
LANG=C
CXX = g++
OBJS =	pxc_y.o pxc_l.o expr.o expr_impl.o expr_misc.o eval.o evalmeta.o \
	checktype.o term.o emit.o emit_context.o symbol.o symbol_table.o \
	cdriver.o pxc_md5.o util.o sort_dep.o
INSTALL_PREFIX = /usr/local

ifndef CXXFLAGS
CXXFLAGS = -Wall -g -O -DNDEBUG
endif
ifeq ($(shell uname), Linux)
  CXXFLAGS += -Wall
endif
ifeq ($(shell uname), Darwin)
  CXXFLAGS += -Wno-unneeded-internal-declaration -Wno-null-conversion
endif

all: release
	# cd tests; $(MAKE)

debug:
	CXXFLAGS="-g -O0" $(MAKE) pxc

release:
	$(MAKE) pxc

%.cpp: %.y
	bison --report=all -d -o $@ $<

%.cpp: %.l
	flex -o$@ $<

pxc_y.o: pxc_y.cpp

pxc_l.o: pxc_l.cpp
	$(CXX) -c $(CXXFLAGS) -Wno-sign-compare $^

pxc: $(OBJS)
	# cd tests; $(MAKE) clean
	# $(CXX) -rdynamic $(CXXFLAGS) -o $@ $^ -ldl -lpthread
	$(CXX) $(CXXFLAGS) -o $@ $^ -ldl -lpthread

test:
	cd tests; $(MAKE)

retest:
	cd tests; $(MAKE) retest

clean:
	rm -f *.o pxc pxc.exe pxc_y.output
	cd tests && $(MAKE) clean
	cd misc && $(MAKE) clean

cleanall: clean
	rm -f pxc_y.cpp pxc_y.hpp pxc_l.cpp

install:
	mkdir -p $(INSTALL_PREFIX)/bin/
	mkdir -p $(INSTALL_PREFIX)/etc/pxc/
	mkdir -p $(INSTALL_PREFIX)/share/pxc/
	rm -rf $(INSTALL_PREFIX)/bin/pxc
	rm -rf $(INSTALL_PREFIX)/etc/pxc.profile
	rm -rf $(INSTALL_PREFIX)/share/pxc
	rm -rf $(INSTALL_PREFIX)/share/pxclib
	if [ "`uname | cut -d '_' -f 1`" = "CYGWIN" ]; then \
		peflags -x0x400000 pxc; \
	fi
	install -m 755 pxc $(INSTALL_PREFIX)/bin/
	cp -R profile/* $(INSTALL_PREFIX)/etc/pxc/
	cp -R pxclib $(INSTALL_PREFIX)/share/pxclib/

uninstall:
	rm -rf $(INSTALL_PREFIX)/bin/pxc
	rm -rf $(INSTALL_PREFIX)/etc/pxc.profile
	rm -rf $(INSTALL_PREFIX)/share/pxc_*
	rm -rf $(INSTALL_PREFIX)/share/pxc
	rm -rf $(INSTALL_PREFIX)/share/pxclib

fix:
	chmod a+rx `find . -type d`
	chmod a+r `find . -type f`
