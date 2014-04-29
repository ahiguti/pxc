
all: hmtest tmtest hashmap chm chm2 tmtest.class hmtest.class

hmtest: hmtest.cpp
	g++ -Wall -g -DNDEBUG -O3 $^ -o $@

tmtest: tmtest.cpp
	g++ -Wall -g -DNDEBUG -O3 $^ -o $@

hashmap: hashmap.cpp
	g++ -Wall -g -DNDEBUG -O3 $^ -o $@

chm: chm.cpp
	g++ -Wall -g -DNDEBUG -O3 $^ -o $@

chm2: chm2.cpp
	g++ -Wall -g -DNDEBUG -O3 $^ -o $@

tmtest.class: tmtest.java
	javac $^

hmtest.class: hmtest.java
	javac $^

clean:
	rm -f hmtest tmtest hashmap chm chm2 *.class

run: all
	/usr/bin/time ./hmtest
	/usr/bin/time ./tmtest
	/usr/bin/time ./hashmap
	/usr/bin/time ./chm
	/usr/bin/time ./chm2
	/usr/bin/time java tmtest
	/usr/bin/time java hmtest
