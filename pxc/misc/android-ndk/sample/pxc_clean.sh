#!/bin/bash

cd `dirname $0`
pushd ./jni/pxsrc/ && ./pxc_clean.sh && popd
if [ "`which ndk-build 2> /dev/null`" != "" ]; then
  ndk-build NDK_DEBUG=0 clean
  ndk-build NDK_DEBUG=1 clean
fi
if [ "`which ant 2> /dev/null`" != "" ]; then
  ant clean
fi
# remove generated java file
rm -f ./src/org/libsdl/app/SDLActivity.java
