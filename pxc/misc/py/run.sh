#!/bin/bash
# pxc -p /etc/pxc/pxc_dynamic.profile -c
pxc -p=/etc/pxc/pxc_dynamic.profile -g -ne pxc.px 
exec python test.py

