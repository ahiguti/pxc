#!/bin/bash

export LIBGL_ALWAYS_SOFTWARE=1
exec pxc -p=unsafe pgl3d.px 

