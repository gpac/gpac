#!/bin/sh

test_begin "vobsub"

if [ $test_skip  = 1 ] ; then
return
fi

myinspect=$TEMP_DIR/inspect.txt
do_test "$GPAC -i $EXTERNAL_MEDIA_DIR/vobsub/vobsub.idx inspect:allp:deep:interleave=false:log=$myinspect -graph -stats" "inspect"
do_hash_test $myinspect "inspect"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/vobsub/vobsub.idx -new $TEMP_DIR/test.mp4" "mp4"
do_hash_test $TEMP_DIR/test.mp4 "mp4"

do_test "$MP4BOX -raw 1 $TEMP_DIR/test.mp4" "dump"
do_hash_test $TEMP_DIR/test_track1.idx "dump-idx"
do_hash_test $TEMP_DIR/test_track1.sub "dump-sub"

test_end
