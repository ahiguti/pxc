#!/bin/bash

cd `dirname $0`

if [ "`uname | cut -d '_' -f 1`" == "CYGWIN" ]; then
	build_target=./windows/x64/Release/pgl3d_demoapp.exe
else
	build_target=./source/demoapp.px.exe
fi

"$build_target" ./res/pgl3d-edit.cnf $*

