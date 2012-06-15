#!/bin/bash

exec gdb --args ../../pxc --no-realpath -w .pxc -p ../common/pxc.profile $*
