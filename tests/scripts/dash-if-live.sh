#!/bin/sh

test_begin "dash-if-live"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264 -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.aac -new $TEMP_DIR/file.mp4" "dash-input-preparation"

do_test "$MP4BOX -dash 1000 -profile dashavc264:live $TEMP_DIR/file.mp4#video $TEMP_DIR/file.mp4#audio -out $TEMP_DIR/file.mpd" "dash-if-live"

do_hash_test $TEMP_DIR/file.mpd "mpd"

do_hash_test $TEMP_DIR/file_dash_track1_init.mp4 "init1"
do_hash_test $TEMP_DIR/file_dash_track2_init.mp4 "init2"
do_hash_test $TEMP_DIR/file_dash_track1_20.m4s "seg1"
do_hash_test $TEMP_DIR/file_dash_track2_20.m4s "seg2"

 
do_playback_test "$TEMP_DIR/file.mpd" "play-dash-if-live"

test_end
