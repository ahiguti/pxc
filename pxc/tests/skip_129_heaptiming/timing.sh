#!/bin/bash

env pxc_timing_param=10000000 ./run.sh heaptest1.px 

javac heaptest1.java
/usr/bin/time java -server -verbose:gc \
	-Xms1000m -Xmx1000m -XX:NewSize=800m -XX:MaxNewSize=800m \
	-XX:MaxTenuringThreshold=15 \
	-XX:SurvivorRatio=2 \
	-XX:TargetSurvivorRatio=90 \
	heaptest1 10000000 100 > j1.log 2>&1

