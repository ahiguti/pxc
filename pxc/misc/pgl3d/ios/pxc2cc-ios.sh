#!/bin/bash

cd `dirname $0` &&
  pushd ../source && \
  pxc -p=../ios/pxc_ios.profile --generate-single-cc -nb \
	--generate-cc=../ios/gen/ \
	demoapp.px && \
  popd

