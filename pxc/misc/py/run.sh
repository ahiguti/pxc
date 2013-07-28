#!/bin/bash
pxc -ne -p /etc/pxc_dynamic.profile --generate-cc -g pyhello.px
exec python test.py
