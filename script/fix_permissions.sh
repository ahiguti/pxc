#!/bin/bash

# ファイルのpermissionを正しく付けなおす
#
# ~/.gitconfig に以下を入れておいたほうがよい
# [core]
#        filemode = false


# cd `dirname $0`

find . -type f -exec chmod 644 {} \;
find . -type d -exec chmod 755 {} \;
find . -name "*.sh" -exec chmod 755 {} \;

