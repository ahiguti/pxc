#!/bin/bash

# exec pxc -g -p=unsafe ./demoapp.px $*
pxc -v=1 -gs -p=unsafe ./demoapp.px $* && exec ./demoapp.px.exe
