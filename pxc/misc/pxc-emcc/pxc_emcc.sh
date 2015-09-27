#!/bin/bash

pxc -gs -nb t1.px
em++ -O2 -I/usr/local/include t1.px.cc -o t1.px.js
