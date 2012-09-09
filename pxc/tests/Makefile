
SHELL=/bin/bash

all:
	$(MAKE) test | tee all.log

force:
	export CXX="$(CXX)"; \
	export SUNJDK="$(SUNJDK)"; \
	for i in test_*; do \
	  pushd $$i > /dev/null; \
	  echo "[$$i]"; \
	  if [ -f ./DONE ]; then \
	    echo "skip"; \
	  elif ! ./run.sh; then \
	    echo -n "FORCE ... "; \
	    $(MAKE) accept; \
	    if ! ./run.sh; then \
	      echo "$$i failed"; \
	      exit 1; \
	    fi; \
	  else \
	    echo "success"; \
	  fi; \
	  touch ./DONE; \
	  popd > /dev/null; \
	done; \
	exit 0;

test:
	export CXX="$(CXX)"; \
	export SUNJDK="$(SUNJDK)"; \
	for i in test_*; do \
	  pushd $$i > /dev/null; \
	  echo "[$$i]"; \
	  if [ -f ./DONE ]; then \
	    echo "skip"; \
	  elif ! ./run.sh; then \
	    echo "$$i failed"; \
	    exit 1; \
	  else \
	    echo "success"; \
	  fi; \
	  touch ./DONE; \
	  popd > /dev/null; \
	done; \
	exit 0;

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
	rm -f all.log
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

