#!/usr/bin/env python 
import os
os.system("../../pxc -p=../common/pxc_dynamic.profile -g -ne pmod1.px")
os.system("mv pmod1.so pxc.so")
import pxc
f = pxc.pmod1_foo()
f.x = 30
print("hoge: " + str(f.hoge()))
f.bar(999, "abcde")
print("fuga: " + str(f.fuga()))
