#!/bin/bash

# ビルドしてから実行する。ビルド時にエラーが出ると更新を待ちリビルドする。
# pxc.profile で container_guard=0 にしているので、デバッグするときは必ず
# debug build ですること

cd `dirname $0`

if [ "`uname | cut -d '_' -f 1`" == "CYGWIN" ]; then
	build_script=./windows/release_build.sh
	build_target=./windows/x64/Release/pgl3d_demoapp.exe
else
	#build_script=./unix/debug_build.sh
	build_script=./unix/release_build.sh
	build_target=./source/demoapp.px.exe
fi

while true; do
	# ビルド失敗するとソースが更新されるまで待ち、
	# 更新されるとリビルドする
	newer_files=`find ./source -name "*.px" -and -newercc \
		"$build_target" 2> /dev/null`
	files_0=`find ./source -name "*.px"`;
	stat_str_0=`stat -c "%z" $files_0 2> /dev/null`;
	if [ -e "$build_target" -a -z "$newer_files" ]; then
		echo "$build_target" is up to date 1>&2
		break
	else
		if "$build_script"; then
			break
		fi
	fi
	while true; do
		sleep 1
		files_1=`find ./source -name "*.px"`;
		stat_str_1=`stat -c "%z" $files_1 2> /dev/null`;
		if [ "$stat_str_0" != "$stat_str_1" ]; then
			break
		fi
	done
done

bgpid=""

trap "echo got SIGINT && kill -9 $bdpid > /dev/null 2>&1" 2
trap "echo got SIGTERM && kill -9 $bdpid > /dev/null 2>&1" 15

echo > var/demoapp.log
tail -f var/demoapp.log &
bgpid=$!
"$build_target" $* > /dev/null 2>&1
kill -9 "$bgpid" > /dev/null 2>&1
wait "$bgpid" > /dev/null 2>&1

