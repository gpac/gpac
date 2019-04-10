#!/bin/sh

TEST_NAME="dash-input-dur"

test_begin "$TEST_NAME"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264 -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.aac -new $TEMP_DIR/file.mp4" "dash-input-preparation"

do_test "$MP4BOX -dash 1000 -rap $TEMP_DIR/file.mp4#video:dur=17 $TEMP_DIR/file.mp4#audio:dur=17 -out $TEMP_DIR/file.mpd" "$TEST_NAME-dash"

do_hash_test $TEMP_DIR/file.mpd "$TEST_NAME-hash-mpd"

do_hash_test $TEMP_DIR/file_dash_track1_init.mp4 "$TEST_NAME-hash-init-segment-video"

do_hash_test $TEMP_DIR/file_dash_track2_init.mp4 "$TEST_NAME-hash-init-segment-audio"

myinspect=$TEMP_DIR/inspect.txt
do_test "$GPAC -i $TEMP_DIR/file.mpd inspect:all:deep:interleave=false:log=$myinspect"
do_hash_test $myinspect "inspect"

test_end
