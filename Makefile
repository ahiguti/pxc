
SHELL = /bin/bash
SUBDIRS = pxc

all:
	for i in $(SUBDIRS); do pushd $$i && make && popd; done

clean:
	for i in $(SUBDIRS); do pushd $$i && make clean && popd; done
	rm -rf dist

install: all
	for i in $(SUBDIRS); do pushd $$i && sudo make install && popd; done

uninstall:
	for i in $(SUBDIRS); do pushd $$i && sudo make uninstall && popd; done

rpms: clean
	mkdir -p dist
	mkdir dist/BUILD dist/RPMS dist/SOURCES dist/SPECS dist/SRPMS
	tar cvfz dist/pxc.tar.gz --exclude=.git pxc
	rpmbuild --define "_topdir `pwd`/dist" -ta dist/pxc.tar.gz

installrpms: uninstallrpms rpms
	sudo rpm -U dist/RPMS/*/*.rpm

uninstallrpms:
	- sudo rpm -e pxc
	- sudo rpm -e pxc-debuginfo

fix:
	chmod a+rx `find . -type d`
	chmod a+r `find . -type f`
