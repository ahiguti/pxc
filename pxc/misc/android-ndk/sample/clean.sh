#!/bin/bash

pushd ./jni/pxsrc/ && ./pxc_clean.sh && popd
ndk-build NDK_DEBUG=0 clean
ndk-build NDK_DEBUG=1 clean
ant clean
# remove generated java file
rm -f ./src/org/libsdl/app/SDLActivity.java
