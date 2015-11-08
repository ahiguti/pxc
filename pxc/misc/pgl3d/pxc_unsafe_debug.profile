incdir=/usr/local/share/pxc/pxc_%{platform}/:/usr/local/share/pxc/pxc_core/:/usr/local/share/pxc/pxc_ext/:/usr/share/pxc/pxc_%{platform}/:/usr/share/pxc/pxc_core/:/usr/share/pxc/pxc_ext/:.
safe_mode=0
cxx=g++ --std=c++11
cflags=-g -O0 -Wall -Wno-unknown-warning-option -Wno-empty-body -Wno-tautological-compare -Wno-overloaded-virtual -Wno-unused -Wno-extern-c-compat -Wno-strict-aliasing -Wno-free-nonheap-object -I/usr/local/include
