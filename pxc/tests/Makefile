
SHELL=/bin/bash

all:
	$(MAKE) test | tee all.log

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

regenerate:
	export CXX="$(CXX)"; \
	for i in test_*; do \
	  pushd $$i > /dev/null; \
	  echo -n "[$$i] "; \
	  if [ -f ./DONE ]; then \
	    echo "skip"; \
	  elif ! ./run.sh; then \
	    echo "$$i failed (regenerate)"; \
	    ../common/copy_exp2_from_log2.sh \
	  else \
	    echo -n ""; \
	  fi; \
	  touch ./DONE; \
	  popd > /dev/null; \
	done; \
	exit 0;

valgrind: valgrind-run valgrind-check

valgrind-run:
	rm -f valgrind.log
	for i in `find . -name "*.exe"`; do \
	  valgrind --suppressions=./common/dlopen.supp $$i \
	    > /dev/null 2>> valgrind.log; \
	done; \
	exit 0;

valgrind-check:
	!( fgrep 'ERROR SUMMARY' valgrind.log | fgrep -v ': 0 errors' )
	!( fgrep 'definitely lost' valgrind.log | fgrep -v ' in 0 blocks' )
	!( fgrep 'indirectly lost' valgrind.log | fgrep -v ' in 0 blocks' )
	!( fgrep 'still reachable' valgrind.log | fgrep -v ' in 0 blocks' )

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
	env TEST_PXC_DEBUG=1 $(MAKE) test

retest: clean test

clean:
	$(MAKE) cleanlogs > /dev/null

cleanlogs:
	rm -rf */*.log */*.log2 */*.log2d */*.log2h */*.exe */*.o */*.class \
	  */*.so */*.pyc */gen */*.px.cc */DONE */*.html */*.js */*.data \
	  */*.mem
	rm -rf work/
	rm -rf all.log
	for i in test_* skip_* fixme_*; do \
	  pushd $$i; \
	  if [ -f Makefile ]; then \
	    $(MAKE) clean; \
	  fi; \
	  popd; \
	done; \

cleanprepare:
	rm -f ../run/*.exe
	rm -f ../run/*.dll

