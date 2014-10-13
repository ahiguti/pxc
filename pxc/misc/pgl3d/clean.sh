#!/bin/bash

cd `dirname $0`
pushd android && ./clean.sh && popd
rm -rf ./ios/gen/*
rm -rf `find ./ios/ -name project.xcworkspace`
rm -rf `find ./ios/ -name xcuserdata`
rm -rf `find . -name .DS_Store`
rm -f *.exe
