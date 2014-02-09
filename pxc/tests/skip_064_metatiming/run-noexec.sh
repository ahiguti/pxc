#!/bin/bash

exec ../../pxc --trim-path=2 -w=../work -p \
	../common/pxc.profile -c --no-execute fib.px
