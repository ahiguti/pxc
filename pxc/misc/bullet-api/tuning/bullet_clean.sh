#!/bin/bash
if [ -f Makefile ]; then make clean; fi
rm -rf lib bin install_manifest.txt bullet.pc build3/xcode4 BulletCOnfig.cmake
rm -rf `find . -name "CMakeFiles"`
rm -rf `find . -name "Makefile"`
rm -rf `find . -name "cmake_install.cmake"`
rm -rf build3/vs2010/obj
rm -rf build3/vs2010/0_*.sdf
rm -rf CMakeCache.txt
rm -f examples/ExampleBrowser/bulletDemo.txt
