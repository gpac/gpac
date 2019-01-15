#!/bin/sh

test_begin "dash-ssix"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264 -new $TEMP_DIR/file.mp4" "dash-input-preparation"

do_test "$MP4BOX -dash 1000 -ssix -profile onDemand $TEMP_DIR/file.mp4#video -out $TEMP_DIR/file.mpd" "dash-ondemand-ssix"

do_hash_test $TEMP_DIR/file.mpd "mpd"
do_hash_test $TEMP_DIR/file_dashinit.mp4 "r1"

do_playback_test "$TEMP_DIR/file.mpd" "play-ondemand-ssix"

test_end
