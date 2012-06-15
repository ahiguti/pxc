
SUBDIRS = perl-PXC-Loader pxc

all: prepare
	for i in $(SUBDIRS); do pushd $$i && make && popd; done

clean: prepare
	for i in $(SUBDIRS); do pushd $$i && make clean && popd; done
	rm -f perl-PXC-Loader/Makefile.old
	rm -rf dist

prepare:
	pushd perl-PXC-Loader && perl Makefile.PL && popd

install: all
	for i in $(SUBDIRS); do pushd $$i && sudo make install && popd; done

uninstall:
	for i in $(SUBDIRS); do pushd $$i && sudo make uninstall && popd; done

rpms: clean
	mkdir -p dist
	mkdir dist/BUILD dist/RPMS dist/SOURCES dist/SPECS dist/SRPMS
	tar cvfz dist/perl-PXC-Loader.tar.gz --exclude=.git perl-PXC-Loader
	tar cvfz dist/pxc.tar.gz --exclude=.git pxc
	rpmbuild --define "_topdir `pwd`/dist" -ta dist/perl-PXC-Loader.tar.gz
	rpmbuild --define "_topdir `pwd`/dist" -ta dist/pxc.tar.gz

installrpms: uninstallrpms rpms
	sudo rpm -U dist/RPMS/*/*.rpm

uninstallrpms:
	- sudo rpm -e perl-PXC-Loader 
	- sudo rpm -e perl-PXC-Loader-debuginfo
	- sudo rpm -e pxc
	- sudo rpm -e pxc-debuginfo

