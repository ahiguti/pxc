incdir=../pxc_%{platform}:../pxc_core:../pxc_ext:.
generate_dynamic=1
cflags=-rdynamic -g -O3 -DNDEBUG -Wall -Wno-strict-aliasing
# cflags=-rdynamic -g -O3 -DNDEBUG -Werror -Wall -Wno-unused
# cflags=-DPXCRT_DBG_RC
# cxx=clang++
#cflags=-rdynamic -g -O3 -DNDEBUG -Werror -Wall -Wno-unused -fno-exceptions
#noexceptions=1
