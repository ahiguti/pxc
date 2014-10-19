incdir=/usr/local/share/pxc/pxc_%{platform}/:/usr/local/share/pxc/pxc_core/:/usr/local/share/pxc/pxc_ext/:/usr/share/pxc/pxc_%{platform}/:/usr/share/pxc/pxc_core/:/usr/share/pxc/pxc_ext/:.
safe_mode=0
container_guard=0
bounds_checking=0
cxx=g++
cflags=-g -O3 -DNDEBUG -Wall -Wno-unused -Wno-strict-aliasing -Wno-free-nonheap-object -I/usr/local/include
generate_dynamic=1
