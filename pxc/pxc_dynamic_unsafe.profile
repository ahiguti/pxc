generate_dynamic=1
incdir=/usr/share/pxc_%{platform}/:/usr/share/pxc_core/:./%{platform}:.
show_warnings
safe_mode=0
cxx=g++
cflags=-g -O3 -DNDEBUG -Wall -Wno-unused -Wno-strict-aliasing
