#!/bin/bash

# exec pxc -g -p=unsafe ./demoapp.px $*
pxc -v=1 -gs -p=./pxc_unsafe.profile ./demoapp.px $* && exec ./demoapp.px.exe 2>&1 | tee /tmp/z
