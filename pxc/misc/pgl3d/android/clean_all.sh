#!/bin/bash

cd `dirname $0`
./pxc_clean.sh
ndk-build NDK_DEBUG=0 clean
ndk-build NDK_DEBUG=1 clean
ant clean
rm -rf obj/*
# remove generated java file
rm -f ./src/org/libsdl/app/SDLActivity.java
