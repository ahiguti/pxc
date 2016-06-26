#!/bin/bash

# exec pxc -g -p=unsafe ./demoapp.px $*
pxc -v=1 -gs -p=./pxc_unsafe_debug.profile ./demoapp.px $* && exec ./demoapp.px.exe 2>&1 | tee /tmp/z
