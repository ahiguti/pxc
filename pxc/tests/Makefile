
SHELL=/bin/bash

all:
	$(MAKE) test | tee all.log

force:
	export CXX="$(CXX)"; \
	for i in test_*; do \
	  pushd $$i > /dev/null; \
	  echo "[$$i] "; \
	  if [ -f ./DONE ]; then \
	    echo " skip"; \
	  elif ! ./run.sh; then \
	    echo -n "FORCE ... "; \
	    $(MAKE) accept; \
	    if ! ./run.sh; then \
	      echo "$$i failed"; \
	      exit 1; \
	    fi; \
	  else \
	    echo -n ""; \
	  fi; \
	  touch ./DONE; \
	  popd > /dev/null; \
	done; \
	exit 0;

test:
	export CXX="$(CXX)"; \
	for i in test_*; do \
	  pushd $$i > /dev/null; \
	  echo -n "[$$i] "; \
	  if [ -f ./DONE ]; then \
	    echo "skip"; \
	  elif ! ./run.sh; then \
	    echo "$$i failed"; \
	    exit 1; \
	  else \
	    echo -n ""; \
	  fi; \
	  touch ./DONE; \
	  popd > /dev/null; \
	done; \
	exit 0;

valgrind:
	rm -f valgrind.log
	for i in `find . -name "*.exe"`; do \
	  valgrind $$i > /dev/null 2>> valgrind.log; \
	done; \
	fgrep 'ERROR SUMMARY' valgrind.log | \
	fgrep -v 'ERROR SUMMARY: 0 errors'; \
	exit 0;

efence:
	rm -f efence.log
	for i in `find . -name "*.exe"`; do \
	  echo -n "$$i: " >> efence.log; \
	  env EF_PROTECT_BELOW=0 ef $$i > /dev/null 2>> efence.log; \
	done; \
	for i in `find . -name "*.exe"`; do \
	  echo -n "$$i: " >> efence.log; \
	  env EF_PROTECT_BELOW=1 ef $$i > /dev/null 2>> efence.log; \
	done; \
	exit 0;

debug:
	env TEST_PXC_PROF=../common/pxc_debug.profile \
	  TEST_PXC_DYNAMIC_PROF=../common/pxc_debug_dynamic.profile \
	  $(MAKE) test

retest: clean test

clean:
	$(MAKE) cleanlogs > /dev/null

cleanlogs:
	for i in test_*; do \
	  pushd $$i > /dev/null; \
	  if [ -f Makefile ]; then \
	    $(MAKE) clean; \
	  fi; \
	  rm -f ./DONE; \
	  popd > /dev/null; \
	done; \
	exit 0;
	rm -f all.log valgrind.log efence.log
	rm -f test_*/*.log
	rm -f test_*/*.out
	rm -f test_*/*.dll
	rm -f test_*/*.idb
	rm -f test_*/*.obj
	rm -f test_*/*.manifest
	rm -f test_*/*.so

cleanprepare:
	rm -f ../run/*.exe
	rm -f ../run/*.dll

