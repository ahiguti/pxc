#!/bin/bash

#export LIBGL_ALWAYS_SOFTWARE=1
exec pxc -p=unsafe ./demoapp.px $*
#exec lldb -- pxc -p=unsafe_debug ./demoapp.px $*
