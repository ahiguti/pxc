incdir=../pxc_%{platform}:../pxc_core:../pxc_test:.
#cflags=-rdynamic -g -O3 -DNDEBUG -Werror -Wall -Wno-unused -Wno-attributes
cflags=-std=c++11 -g -O3 -DNDEBUG -Wall -Wno-unused -Wno-attributes -I/usr/local/include
safe_mode=0
# cflags=-DPXCRT_DBG_RC
# cxx=g++
#cxx=clang++
