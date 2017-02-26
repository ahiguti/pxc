#!/bin/bash

cd `dirname $0`
./pxc_clean.sh
if [ "`which ndk-build 2> /dev/null`" != "" ]; then
	ndk-build NDK_DEBUG=0 clean
	ndk-build NDK_DEBUG=1 clean
fi
if [ "`which ant 2> /dev/null`" != "" ]; then
	ant clean
fi
rm -rf obj/*
# remove generated java file
# rm -f ./src/org/libsdl/app/SDLActivity.java
