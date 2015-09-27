#!/bin/bash

if [ -h $1 ]; then
  exit
fi

if ! grep -q 'import test_common' $1; then
  echo "import test_common \"\";" > $1.tmp
  cat $1 >> $1.tmp
  mv $1.tmp $1
fi
