#!/bin/bash

unset LC_ALL LC_COLLATE LC_CTYPE LC_MESSAGES LC_MONETARY LC_NUMERIC LC_TIME
unset LANG
ulimit -c 0

if [ "$1" != "" ]; then
  TESTS="$1"
  shift
  OPTS=$*
else
  TESTS=`echo *.px | sort`
fi

if [ "$TEST_PXC_PROF" == "" ]; then
  if [ "$TEST_PXC_DEBUG" == "1" ]; then
    TEST_PXC_PROF=../common/pxc_debug.profile
  else
    TEST_PXC_PROF=../common/pxc.profile
  fi
fi

if [ "$TEST_PXC_DYNAMIC_PROF" == "" ]; then
  if [ "$TEST_PXC_DEBUG" == "1" ]; then
    TEST_PXC_DYNAMIC_PROF=../common/pxc_debug_dynamic.profile
  else
    TEST_PXC_DYNAMIC_PROF=../common/pxc_dynamic.profile
  fi
fi

total=0
err=0
errnames=''
export MUDFLAP_OPTIONS=-viol-segv
for i in $TESTS; do
  bn=`basename $i .px`
  # echo "$bn";
  echo -n ".";
  if [ "$i" != "pxcrt.px" ]; then
    ../../pxc -ne -gs --trim-path=2 -w=../work -p="$TEST_PXC_PROF" $OPTS "$i" \
    	> $bn.log 2> $bn.log2
    g++ --std=c++11 -O -I/usr/local/include $i.cc -lpthread -lboost_system \
    	-lboost_thread -o $i.cc.exe
    ./$i.cc.exe >> $bn.log 2>> $bn.log2
    if [ ! "$?" ]; then
      echo "$bn failed"
    fi
  else
    touch $bn.log
  fi
  bn=`basename $i .px`
  if ! diff -u $bn.exp $bn.log > /dev/null 2>&1; then
    echo
    diff -u $bn.exp $bn.log 2>&1
    echo "error output:"
    cat $bn.log2 2> /dev/null
    echo "$bn failed"
    err=$((err + 1))
    errnames="$errnames $bn"
    # exit 1
  elif [ -f "$bn.exp2" ]; then
    if ! diff -u $bn.exp2 $bn.log2 > /dev/null 2>&1; then
      echo
      diff -u $bn.exp2 $bn.log2 2>&1
      echo "$bn failed";
      err=$((err + 1))
      errnames="$errnames $bn"
      # exit 1
    fi
  fi
  total=$((total + 1))
done
echo
if [ "$err" != 0 ]; then
  echo "FAILED$errnames"
  echo "FAILED $err of $total"
  exit 1
fi
exit 0
