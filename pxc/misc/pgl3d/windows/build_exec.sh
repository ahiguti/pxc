#!/bin/bash
cd `dirname $0`
./release_build.sh && \
cd .. && \
./windows/x64/Release/pgl3d_demoapp.exe 2>&1 | tee /tmp/z
