#!/bin/bash

cd `dirname $0`

if [ "`uname | cut -d '_' -f 1`" == "CYGWIN" ]; then
        # enables container_guard
	build_script=./windows/debug_build.sh
	build_target=./windows/x64/Release/pgl3d_demoapp.exe
else
	#build_script=./unix/debug_build.sh
	build_script=./unix/debug_build.sh
	build_target=./source/demoapp.px.exe
fi

exec "$build_script"

