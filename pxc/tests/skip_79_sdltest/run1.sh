#!/bin/bash

unset LC_ALL LC_COLLATE LC_CTYPE LC_MESSAGES LC_MONETARY LC_NUMERIC LC_TIME
unset LANG
ulimit -c 0

if [ "$1" != "" ]; then
  TESTS="$1"
  shift
  OPTS=$*
else
  TESTS=''
fi

if [ "$TEST_PXC_PROF" == "" ]; then
  TEST_PXC_PROF=../common/pxc.profile
fi

total=0
err=0
errnames=''
export MUDFLAP_OPTIONS=-viol-segv
for i in $TESTS; do
  bn=`basename $i .px`
  # ../../pxc -g -ne -v 1 --no-realpath -w .pxc -p "$TEST_PXC_PROF" $OPTS "$i"
  ../../pxc -v 0 --no-realpath -w .pxc -p "$TEST_PXC_PROF" $OPTS "$i"
  bn=`basename $i .px`
  total=$((total + 1))
done
if [ "$err" != 0 ]; then
  echo "FAILED$errnames"
  echo "FAILED $err of $total"
  exit 1
fi
exit 0
