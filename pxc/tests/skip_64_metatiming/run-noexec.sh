#!/bin/bash

exec ../../pxc --no-realpath -w .pxc -p \
	../common/pxc.profile -c --no-execute fib.px
