incdir=/usr/local/share/pxc/pxc_%{platform}/:/usr/local/share/pxc/pxc_core/:/usr/local/share/pxc/pxc_ext/:/usr/share/pxc/pxc_%{platform}/:/usr/share/pxc/pxc_core/:/usr/share/pxc/pxc_ext/:.
show_warnings
safe_mode=1
cxx=g++ --std=c++11
cflags=-g -O0 -DDEBUG -Wall -Wno-unused -Wno-strict-aliasing -Wno-free-nonheap-object -I/usr/local/include
generate_dynamic=1
