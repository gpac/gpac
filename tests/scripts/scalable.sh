#!/bin/sh

scalable_test()
{
if [ $1 = "svc" ] ; then
sourcefile="$EXTERNAL_MEDIA_DIR/scalable/svc.264"
else
sourcefile="$EXTERNAL_MEDIA_DIR/scalable/shvc.265"
fi

mp4file="$TEMP_DIR/$1.mp4"
splitfile="$TEMP_DIR/$1-split.mp4"
mergefile="$TEMP_DIR/$1-merge.mp4"

$MP4BOX -add $sourcefile -new $mp4file 2> /dev/null

test_begin $1 "split" "merge" "play"
do_test "$MP4BOX -add "$sourcefile:svcmode=split" -new $splitfile" "Split"
do_hash_test $splitfile "split"

do_test "$MP4BOX -add "$splitfile:svcmode=merge" -new $mergefile" "Merge"
do_hash_test $mergefile "merge"

#compare hashes of source and merge
do_compare_file_hashes $mp4file $mergefile
rv=$?

if [ $rv != 0 ] ; then
result="Hash is not the same between source content and merge content"
fi

do_playback_test "$splitfile" "play"

test_end

}

#test svc
scalable_test "svc" &

#test shvc
scalable_test "shvc"&
