#!/bin/bash

for i in *.exp2; do cp `basename $i .exp2`.log2 $i; done
