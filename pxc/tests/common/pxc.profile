#incdir=../pxc_%{platform}:../pxc_generic:.
incdir=../pxc_generic:.
safe_mode=0
cflags=-g -O3 -DNDEBUG -DNO_LOCAL_POOL=1
#cflags=-g -O3 -DNDEBUG
# -Werror -Wall -Wno-unused -Wno-attributes
#cflags=-g -O3 -DNDEBUG -Wall -Wno-unused -Wno-attributes
#cflags=-g -O0 -DNDEBUG -Wall -Wno-unused -Wno-attributes
# cflags=-DPXCRT_DBG_RC
cxx=g++
# cxx=clang++
# cflags=-g -O3 -DNDEBUG -DNO_LOCAL_POOL=1 -fno-exceptions
# noexceptions=1
