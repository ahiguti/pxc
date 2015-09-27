#!/bin/bash

if ! grep -q 'namespace' $1; then
  echo "namespace `basename $1 .px`;" > $1.tmp
  cat $1 >> $1.tmp
  mv $1.tmp $1
fi
