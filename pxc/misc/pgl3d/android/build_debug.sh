#!/bin/bash

cd `dirname $0` && \
./apply_sdl_diff.sh && \
./pxc2cc-android.sh && \
ndk-build V=1 NDK_DEBUG=1 && \
ant uninstall && \
ant debug install
