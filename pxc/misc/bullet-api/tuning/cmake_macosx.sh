CXX=clang++ cmake \
	-DCMAKE_CXX_FLAGS_DEBUG:STRING="--std=c++11 -O -DDEBUG -g" . \
	-DCMAKE_CXX_FLAGS_RELEASE:STRING="--std=c++11 -O3 -DNDEBUG -g" .
