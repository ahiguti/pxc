incdir=../pxc_%{platform}:../pxc_generic:.
#cflags=-rdynamic -g -O3 -DNDEBUG -Werror -Wall -Wno-unused -Wno-attributes
cflags=-std=c++11 -g -O3 -DNDEBUG -Wall -Wno-unused -Wno-attributes
# cflags=-DPXCRT_DBG_RC
# cxx=g++
#cxx=clang++
