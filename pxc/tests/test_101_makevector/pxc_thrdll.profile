incdir=../pxc_%{platform}:../pxc_generic:.
generate_dynamic=1
emit_threaded_dll=thrdll_info::info
cflags=-rdynamic -g -O3 -DNDEBUG -Werror -Wall -Wno-unused
