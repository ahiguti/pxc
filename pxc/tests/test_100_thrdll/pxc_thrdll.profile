incdir=../pxc_core:../pxc_ext:.
generate_dynamic=1
emit_threaded_dll=thrdll_info::info
cflags=-rdynamic -g -O3 -DNDEBUG -Wall -Wno-unused -I/usr/local/include
# -Werror
