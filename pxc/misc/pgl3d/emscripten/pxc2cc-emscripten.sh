#!/bin/bash

cd `dirname $0` &&
  pushd .. && \
  pxc -p=./emscripten/pxc_emscripten.profile --generate-single-cc -nb \
    --generate-cc=./emscripten/gen/ \
    demoapp.px && \
  em++ -std=c++11 -O2 -I./emscripten/ \
    -s USE_SDL=2 -s USE_SDL_IMAGE=2 -s USE_SDL_TTF=2 -s USE_FREETYPE=1 \
    -s USE_BULLET=1 -s TOTAL_MEMORY=100000000 \
    -s EXPORTED_FUNCTIONS="['_main','_load_fs_cb','_save_fs_cb']" \
    -Wno-tautological-compare \
    ./emscripten/gen/demoapp.px.cc -o ./emscripten/gen/demoapp.html \
    --preload-file dpat.png --preload-file pmpat.png \
    --preload-file mplus-1m-bold.ttf \
    --preload-file pgl3d.cnf --emrun && \
  popd &&
  emrun --browser=firefox gen/demoapp.html 2>&1 | tee /tmp/z

    # -s NO_EXIT_RUNTIME=1 \
