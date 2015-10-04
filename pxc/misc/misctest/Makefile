
CXX = g++ -std=c++0x
CXXFLAGS = -g -O3 -DNDEBUG

all: vec
#mftest

vec: vec.cpp
	$(CXX) $(CXXFLAGS) vec.cpp -o vec

mftest: mftest.cpp
	$(CXX) -g -O0 -fmudflapth -lmudflapth mftest.cpp -o mftest -lpthread

clean:
	rm -f vec mftest

timing:
	./vec.px
	/usr/bin/time ./vec.px
	/usr/bin/time ./vec
