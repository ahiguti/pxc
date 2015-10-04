all: htiming.exe mtiming.exe tmtiming.class

htiming.exe:
	g++ -O3 -g -Wall htiming.cpp -o htiming.exe

mtiming.exe:
	g++ -O3 -g -Wall mtiming.cpp -o mtiming.exe

tmtiming.class:
	javac tmtiming.java

clean:
	rm -f *.exe a.out *.class 

run: all
	/usr/bin/time ./tmtiming.px
	/usr/bin/time java -verbose:gc -Xmx3000m -Xms3000m tmtiming
	/usr/bin/time ./mtiming.exe
