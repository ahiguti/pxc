#!/bin/bash

cd `dirname $0`
# remove harmful System.loadLibrary() calls
cat ./jni/SDL2/android-project/src/org/libsdl/app/SDLActivity.java \
  | sed -e 's/System\.loadLibrary.*/\/\//g' \
  > ./src/org/libsdl/app/SDLActivity.java
