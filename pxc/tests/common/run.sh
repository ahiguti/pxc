#!/bin/bash

unset LANG

if [ "$1" != "" ]; then
  TESTS="$1"
else
  TESTS=`echo *.px | sort`
fi

total=0
err=0
errnames=''
for i in $TESTS; do
  bn=`basename $i .px`
  echo -n ".";
  if [ "$i" != "pxcrt.px" ]; then
    ../../pxc --no-realpath -w .pxc -p ../common/pxc.profile "$i" \
    	> $bn.log 2> $bn.log2
  else
    touch $bn.log
  fi
  bn=`basename $i .px`
  if ! diff -u $bn.log $bn.exp > /dev/null 2>&1; then
    echo
    diff -u $bn.log $bn.exp 2>&1
    echo "error output:"
    cat $bn.log2 2> /dev/null
    echo "$bn failed"
    err=$((err + 1))
    errnames="$errnames $bn"
    # exit 1
  elif [ -f "$bn.exp2" ]; then
    if ! diff -u $bn.log2 $bn.exp2 > /dev/null 2>&1; then
      echo
      diff -u $bn.log2 $bn.exp2 2>&1
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
