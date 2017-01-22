#!/bin/bash

cd `dirname $0`
pushd android && ./clean_all.sh && popd
pushd windows && ./clean_all.sh && popd
rm -rf ./ios/gen/*
rm -rf ./emscripten/gen/*
rm -rf `find ./ios/ -name project.xcworkspace`
rm -rf `find ./ios/ -name xcuserdata`
rm -rf `find . -name .DS_Store`
rm -f source/*.exe source/*.o source/*.cc
rm -f res/*.ttf
rm -f var/*.raw var/*.log
rm -f glprog.*.bin
rm -rf dist/
