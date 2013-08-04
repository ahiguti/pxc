#!/bin/bash
pxc -c -ne -p /etc/pxc_dynamic.profile --generate-cc -g pxc_python.px
exec python test.py
