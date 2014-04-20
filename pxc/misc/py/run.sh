#!/bin/bash
# pxc -p /etc/pxc/pxc_dynamic.profile -c
pxc -p=/etc/pxc/pxc_dynamic_unsafe.profile -g -ne pxc_python.px
exec python test.py
