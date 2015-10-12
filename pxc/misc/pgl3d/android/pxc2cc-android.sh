#!/bin/bash

cd `dirname $0` &&
  pushd .. && \
  pxc -p=./android/pxc_android.profile --generate-single-cc \
  	--generate-cc=./android/jni/pxsrc/gen/ -nb demoapp.px && \
  popd
