incdir=../pxc_core:../pxc_ext:../pxc_test:.
safe_mode=0
cxx=g++ --std=c++11
cflags=-rdynamic -g -O0 -Wall -Wno-unused -DNO_LOCAL_POLL -I/usr/local/include
# incdir=../pxc_%{platform}:../pxc_core:.
# cflags=-std=c++11 -rdynamic -g -O0 -Wall -Wno-unused
# -fmudflapth
# ldflags=-lmudflapth
# cflags=-DPXCRT_DBG_RC
#cflags=-DPXCRT_DBG_COND
# cxx=clang++
