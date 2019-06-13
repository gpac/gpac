#!/bin/sh

test_begin "dash-live"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264 -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.aac -new $TEMP_DIR/file.mp4" "dash-input-preparation"

do_test "$MP4BOX -dash 1000 -profile live $TEMP_DIR/file.mp4#video $TEMP_DIR/file.mp4#audio -out $TEMP_DIR/file.mpd" "dash-live"
do_hash_test "$TEMP_DIR/file.mpd" "mpd"
do_hash_test "$TEMP_DIR/file_dash_track1_init.mp4" "init1"
do_hash_test "$TEMP_DIR/file_dash_track2_init.mp4" "init2"
do_hash_test "$TEMP_DIR/file_dash_track1_20.m4s" "seg1"
do_hash_test "$TEMP_DIR/file_dash_track2_20.m4s" "seg2"

myinspect=$TEMP_DIR/inspect.txt
do_test "$GPAC -i $TEMP_DIR/file.mpd inspect:allp:deep:interleave=false:dur=2/1:log=$myinspect"
do_hash_test $myinspect "inspect"

test_end
