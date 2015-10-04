incdir=../pxc_%{platform}:../pxc_core:../pxc_ext:.
generate_dynamic=1
cxx=g++ --std=c++11
cflags=-rdynamic -g -O0 -Werror -Wall -Wno-unused -Wno-attribute -I/usr/local/include
# -fmudflapth
# libs=-lmudflapth
# cflags=-DPXCRT_DBG_RC
# cxx=clang++
