#!/bin/bash

cd `dirname $0` &&
  pushd .. && \
  pxc -p=./windows/pxc_windows.profile --generate-single-cc \
	--generate-cc=./windows/gen/ \
	demoapp.px && \
  popd

