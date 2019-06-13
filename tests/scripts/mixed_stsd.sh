#!/bin/sh

test_begin "mixed_stsd"

if [ $test_skip  = 1 ] ; then
return
fi


mp4file=$TEMP_DIR/file.mp4
#generate playlist in current dir since we put relative path in it
plist=plist-params.m3u
echo "" > $plist
echo "$MEDIA_DIR/auxiliary_files/count_video.cmp" >> $plist
echo "$MEDIA_DIR/auxiliary_files/enst_video.h264" >> $plist

do_test "$GPAC -i $plist -o $mp4file" "create-mp4"
do_hash_test $mp4file "create"

myinspect=$TEMP_DIR/inspect.txt
do_test "$GPAC -i $mp4file inspect:allp:deep:log=$myinspect"
do_hash_test $myinspect "inspect"

rm $plist

test_end

