#!/bin/bash

cd `dirname $0`
./apply_sdl_diff.sh
pushd ./jni/pxsrc/ && ./pxc2cc.sh && popd
ndk-build NDK_DEBUG=0
ant uninstall
ant release install
