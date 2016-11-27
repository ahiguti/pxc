#!/bin/bash

cd `dirname $0`
if [ "`uname | cut -d '_' -f 1`" == "CYGWIN" ]; then
	./windows/release_build.sh && \
	  exec ./windows/x64/Release/pgl3d_demoapp.exe $* 2>&1 | \
	  tee ./var/pgl3d.log
else
	./unix/release_build.sh && \
	  exec ./source/demoapp.px.exe $* 2>&1 | \
	  tee ./var/pgl3d.log
fi
