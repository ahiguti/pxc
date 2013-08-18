incdir=../pxc_%{platform}:../pxc_generic:.
cflags=-g -O3 -DNDEBUG -Werror -Wall -Wno-unused -Wno-attributes
#cflags=-g -O3 -DNDEBUG -Wall -Wno-unused -Wno-attributes
#cflags=-g -O0 -DNDEBUG -Wall -Wno-unused -Wno-attributes
# cflags=-DPXCRT_DBG_RC
cxx=g++
# cxx=clang++
