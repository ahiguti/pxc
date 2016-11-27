#!/bin/bash

cd `dirname $0` && cd ../source && \
  exec pxc -v=1 -gs -p=../unix/pxc_unsafe.profile ./demoapp.px $*
