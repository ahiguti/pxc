incdir=../pxclib:.
generate_dynamic=1
cflags=--std=c++11 -rdynamic -g -O3 -DNDEBUG -Wall -Wno-strict-aliasing -I/usr/local/include
ldflags=-L/usr/local/lib
# cflags=-rdynamic -g -O3 -DNDEBUG -Werror -Wall -Wno-unused
# cflags=-DPXCRT_DBG_RC
# cxx=clang++
#cflags=-rdynamic -g -O3 -DNDEBUG -Werror -Wall -Wno-unused -fno-exceptions
#noexceptions=1
