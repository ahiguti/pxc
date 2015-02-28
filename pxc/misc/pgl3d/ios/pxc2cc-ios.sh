#!/bin/bash

cd `dirname $0` &&
  pushd .. && \
  pxc -p=./ios/pxc_ios.profile --generate-single-cc --generate-cc=./ios/gen/ \
	demoapp.px && \
  popd

