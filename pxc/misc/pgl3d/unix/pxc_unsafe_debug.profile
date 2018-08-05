incdir=/usr/local/share/pxclib/:/usr/share/pxclib/:.
safe_mode=0
cxx=g++ --std=c++11
cflags=-g -O0 -DDEBUG -Wall -Wno-unknown-warning-option -Wno-empty-body -Wno-tautological-compare -Wno-overloaded-virtual -Wno-unused -Wno-extern-c-compat -Wno-strict-aliasing -Wno-free-nonheap-object -I/usr/local/include -I/usr/include/SDL2 -I/build/boost -I/build/bullet/src
ldflags=-L/build/bullet/src/LinearMath -L/build/bullet/src/BulletCollision -L/build/bullet/src/BulletDynamics
