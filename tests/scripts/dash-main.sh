#!/bin/sh

rm -f "$TEMP_DIR/file.mpd"
rm -f "$TEMP_DIR/dash_ctx"

test_begin "dash-mainprofile"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264  -new $TEMP_DIR/file.mp4" "dash-input-preparation"

do_test "$MP4BOX -dash 1000 $TEMP_DIR/file.mp4#video -profile main -segment-name "test-\$Number\$" -out $TEMP_DIR/file.mpd" "dash-mainprofile"

do_hash_test $TEMP_DIR/file.mpd "mpd"
do_hash_test $TEMP_DIR/test-init.mp4 "init"
do_hash_test $TEMP_DIR/test-10.m4s "seg"

myinspect=$TEMP_DIR/inspect.txt
do_test "$GPAC -i $TEMP_DIR/file.mpd inspect:allp:deep:interleave=false:log=$myinspect"
do_hash_test $myinspect "inspect"

test_end
