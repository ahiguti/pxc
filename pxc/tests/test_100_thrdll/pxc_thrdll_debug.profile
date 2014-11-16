incdir=../pxc_core:../pxc_ext:.
generate_dynamic=1
emit_threaded_dll=thrdll_info::info
cflags=--std=c++11 -rdynamic -g -O0 -Wall -Wno-unused -DNO_LOCAL_POOL -I/usr/local/include
# -Werror
