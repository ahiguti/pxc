#!/bin/bash
cd `dirname $0`
mkdir -p ./x64/Release
rm -f ./x64/Release/*.exe
cp -af c:/build/ext/*.dll ./x64/Release/
cp -af c:/build/ext/*.ttf ../res/
./pxc2cc-windows-debug.sh && chcp.com 65001 && \
"c:/Program Files (x86)/MSBuild/14.0/Bin/amd64/MSBuild.exe" pgl3d_demoapp.sln /t:Build /p:"Configuration=Release" /p:"Platform=x64" /p:VCTargetsPath="C:\Program Files (x86)\MSBuild\Microsoft.Cpp\v4.0\v140"
exit $?
# if [ "$?" != "0" ]; then exit $?; fi
