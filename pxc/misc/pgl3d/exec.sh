#!/bin/bash

cd `dirname $0`
if [ "`uname | cut -d '_' -f 1`" == "CYGWIN" ]; then
	exec ./windows/x64/Release/pgl3d_demoapp.exe 2>&1 | tee /tmp/z
else
	exec ./demoapp.px.exe 2>&1 | tee /tmp/z
fi
