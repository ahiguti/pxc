#!/bin/bash

if [ "$TEST_PXC_PROF" == "" ]; then
  # TEST_PXC_PROF=../common/pxc.profile
  TEST_PXC_PROF=../common/pxc_debug.profile
fi
export MUDFLAP_OPTIONS=-viol-segv

exec gdb --args ../../pxc --trim-path=2 -w=../work -p="$TEST_PXC_PROF" $*

