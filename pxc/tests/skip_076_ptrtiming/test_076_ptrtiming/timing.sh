#!/bin/bash

env pxc_timing_loop=10000 ./run.sh t1.px
env pxc_timing_loop=200 ./run.sh t2.px

javac foo.java
javac heaptm.java
/usr/bin/time java foo > j1-1.log
/usr/bin/time java -Xincgc foo > j1-2.log
/usr/bin/time java heaptm > j2-1.log
/usr/bin/time java -Xincgc heaptm > j2-2.log
