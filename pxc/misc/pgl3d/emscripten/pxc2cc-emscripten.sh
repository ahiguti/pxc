#!/bin/bash

cd `dirname $0`
cd ..

if ! [ -f mplus-1m-bold.ttf ]; then
  cp -f /usr/share/fonts/mplus/mplus-1m-bold.ttf .
fi

pxc -p=./emscripten/pxc_emscripten.profile --generate-single-cc -nb \
    --generate-cc=./emscripten/gen/ \
    demoapp.px && \
  em++ -std=c++11 -O2 -I./emscripten/ \
    -s USE_SDL=2 -s USE_SDL_IMAGE=2 -s USE_SDL_TTF=2 -s USE_FREETYPE=1 \
    -s USE_BULLET=1 -s TOTAL_MEMORY=200000000 \
    -s EXPORTED_FUNCTIONS="['_main','_load_fs_cb','_save_fs_cb']" \
    -Wno-tautological-compare \
    ./emscripten/gen/demoapp.px.cc -o ./emscripten/gen/demoapp.html \
    --preload-file dpat.png --preload-file pmpat.png \
    --preload-file cube_right1.png \
    --preload-file cube_left2.png \
    --preload-file cube_top3.png \
    --preload-file cube_bottom4.png \
    --preload-file cube_front5.png \
    --preload-file cube_back6.png \
    --preload-file mplus-1m-bold.ttf \
    --preload-file pgl3d.cnf --emrun &&
  emrun --browser=firefox emscripten/gen/demoapp.html 2>&1 | tee /tmp/z

