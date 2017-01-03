#!/bin/bash

cd `dirname $0`
if [ "`uname | cut -d '_' -f 1`" == "CYGWIN" ]; then
	build_script=./windows/release_build.sh
	build_target=./windows/x64/Release/pgl3d_demoapp.exe
else
	build_script=./unix/release_build.sh
	build_target=./source/demoapp.px.exe
fi

newer_files=`find ./source -name "*.px" -and -newercc "$build_target" \
	2> /dev/null`
if [ -e "$build_target" -a -z "$newer_files" ]; then
	echo "$build_target" is up to date 1>&2
else
	if ! "$build_script"; then
		exit 1
	fi
fi

rm -rf dist
mkdir dist
mkdir dist/var
cp -a res dist/
cp -a "$build_target" dist/
cp -a ./windows/x64/Release/*.dll dist/

