#!/bin/bash

for i in *.exp; do cp `basename $i .exp`.log $i; done
