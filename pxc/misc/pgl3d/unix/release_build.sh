#!/bin/bash

exec pxc -v=1 -gs -p=./pxc_unsafe.profile ./demoapp.px $*
