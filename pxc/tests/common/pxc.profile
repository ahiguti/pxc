incdir=../pxclib:../common:.
safe_mode=0
cxx=ccache g++ --std=gnu++11
# cxx=clang++
cflags=-g -O3 -DNDEBUG -Wall -Wno-unused -Wno-strict-aliasing -I/usr/local/include
ldflags=-L/usr/local/lib
# -Werror
 # -Wno-error=unused-local-typedefs # -Wno-unused -Wno-attributes
# cflags=-DPXCRT_DBG_RC
# cxx=clang++
# cflags=-g -O3 -DNDEBUG -DNO_LOCAL_POOL=1 -fno-exceptions
# noexceptions=1
