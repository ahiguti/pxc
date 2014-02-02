generate_dynamic=1
incdir=/usr/share/pxc_%{platform}/:/usr/share/pxc_generic/:./%{platform}:.
show_warnings
safe_mode=1
cxx=g++
cflags=-g -O3 -DNDEBUG -Werror -Wall -Wno-unused -Wno-attributes
