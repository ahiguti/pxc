
LANG=C
#CXX = g++34
CXX = g++
#CXXFLAGS = -Wall -g -Os -DNDEBUG
CXXFLAGS = -Wall -g -O3 -DNDEBUG
#CXXFLAGS = -Wall -g -pg -O -DNDEBUG
#CXXFLAGS = -Wall -g -O0
OBJS =	pxc_y.o pxc_l.o expr.o expr_impl.o expr_misc.o eval.o evalmeta.o \
	checktype.o term.o emit.o emit_context.o symbol_table.o cdriver.o \
	pxc_md5.o util.o sort_dep.o
INSTALL_PREFIX=/usr/local

all: pxc
	cd tests; $(MAKE)

%.cpp: %.y
	bison --report=all -d -o $@ $<

%.cpp: %.l
	flex -o$@ $<

pxc_y.o: pxc_y.cpp

pxc_l.o: pxc_l.cpp
	$(CXX) -c $(CXXFLAGS) -Wno-sign-compare $^

pxc: $(OBJS)
	cd tests; $(MAKE) clean
	$(CXX) -rdynamic $(CXXFLAGS) -o $@ $^ -ldl -lpthread

retest:
	cd tests; $(MAKE) retest

clean:
	rm -f *.o pxc pxc_y.output
	cd tests && $(MAKE) clean
	cd misc && $(MAKE) clean

cleanall: clean
	rm -f pxc_y.cpp pxc_y.hpp pxc_l.cpp

#install:
#	mkdir -p $(INSTALL_PREFIX)/bin/
#	mkdir -p $(INSTALL_PREFIX)/share/pxc/
#	install -m 755 pxc $(INSTALL_PREFIX)/bin/
#	install -m 644 tests/common/*.px $(INSTALL_PREFIX)/share/pxc/

#uninstall:
#	rm -rf $(INSTALL_PREFIX)/bin/pxc
#	rm -rf $(INSTALL_PREFIX)/share/pxc/

