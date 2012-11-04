
CXX = g++ -std=c++0x
CXXFLAGS = -g -O3 -DNDEBUG

all: vec

vec: vec.cpp
	$(CXX) $(CXXFLAGS) vec.cpp -o vec

clean:
	rm -f vec

timing:
	./vec.px
	/usr/bin/time ./vec.px
	/usr/bin/time ./vec
