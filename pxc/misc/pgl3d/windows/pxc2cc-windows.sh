#!/bin/bash

cd `dirname $0` &&
  pushd ../source && \
  pxc -p=../windows/pxc_windows.profile --generate-single-cc -nb \
	--generate-cc=../windows/gen/ \
	demoapp.px && \
  popd

