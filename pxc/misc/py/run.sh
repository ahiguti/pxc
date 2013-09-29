#!/bin/bash
# pxc -p /etc/pxc_dynamic.profile -c
pxc -p /etc/pxc_dynamic.profile -g -ne pxc_python.px
exec python test.py
