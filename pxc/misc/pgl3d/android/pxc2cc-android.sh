#!/bin/bash

cd `dirname $0` &&
  pushd ../source && \
  pxc -p=../android/pxc_android.profile --generate-single-cc -nb \
  	--generate-cc=../android/jni/pxsrc/gen/ \
	demoapp.px && \
  popd
