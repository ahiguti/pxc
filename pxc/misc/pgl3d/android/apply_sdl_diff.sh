#!/bin/bash

cd `dirname $0`
# remove harmful System.loadLibrary() calls
cat ./pgl3d-extlib/SDL2/android-project/src/org/libsdl/app/SDLActivity.java \
  | sed -e 's/System\.loadLibrary.*/\/\//g' \
  > ./src/org/libsdl/app/SDLActivity.java
