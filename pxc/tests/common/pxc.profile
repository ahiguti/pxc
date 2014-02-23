incdir=../pxc_core:../pxc_ext:.
safe_mode=0
cxx=g++
cflags=-g -O3 -DNDEBUG -Wall -Wno-unused -Wno-strict-aliasing
# -Werror
 # -Wno-error=unused-local-typedefs # -Wno-unused -Wno-attributes
# cflags=-DPXCRT_DBG_RC
# cxx=clang++
# cflags=-g -O3 -DNDEBUG -DNO_LOCAL_POOL=1 -fno-exceptions
# noexceptions=1
# incdir=../pxc_%{platform}:../pxc_core:.
