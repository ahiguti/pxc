#!/bin/bash

#pxc -p=unsafe -gs -nb sdltest3.px && \
#em++ -O2 -I. -s USE_SDL=2 -s USE_SDL_IMAGE=2 -s USE_SDL_TTF=2 sdltest3.px.cc \
#  -o test3.html

#pxc -p=unsafe -gs -nb sdlimagetest.px && \
#em++ -std=c++11 -O2 -I. -s USE_SDL=2 -s USE_SDL_IMAGE=2 -s USE_SDL_TTF=2 \
#  sdlimagetest.px.cc --emrun -o test4.html --preload-file img-c256.jpg &&
#emrun --browser=firefox test4.html

pxc -p=./emcc.profile -gs -nb gears3.px && \
em++ -std=c++11 -O2 -I. -s USE_SDL=2 -s USE_SDL_IMAGE=2 -s USE_SDL_TTF=2 \
  gears3.px.cc --emrun -o test5.html --preload-file img-c256.jpg &&
emrun --browser=firefox test5.html
  # -s FULL_ES2=1 \
