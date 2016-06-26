#!/bin/bash
cd `dirname $0`
./build_release.sh && \
cd .. && \
exec ./windows/x64/Release/pgl3d_demoapp.exe
