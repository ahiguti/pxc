
all: hmtest tmtest hashmap tmtest.class hmtest.class

hmtest: hmtest.cpp
	g++ -Wall -g -DNDEBUG -O3 $^ -o $@

tmtest: tmtest.cpp
	g++ -Wall -g -DNDEBUG -O3 $^ -o $@

hashmap: hashmap.cpp
	g++ -Wall -g -DNDEBUG -O3 $^ -o $@

tmtest.class: tmtest.java
	javac $^

hmtest.class: hmtest.java
	javac $^

clean:
	rm -f hmtest tmtest hashmap *.class

run: all
	/usr/bin/time ./hmtest
	/usr/bin/time ./tmtest
	/usr/bin/time ./hashmap
	/usr/bin/time java tmtest
	/usr/bin/time java hmtest
