#!/bin/bash

cd `dirname $0` && cd ../source && \
  exec pxc -v=1 -gs -p=../unix/pxc_unsafe_debug.profile ./demoapp.px $*
