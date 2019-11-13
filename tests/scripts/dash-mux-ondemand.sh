#!/bin/sh

test_begin "dash-mux-ondemand"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264 -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.aac -new $TEMP_DIR/file.mp4" "dash-input-preparation"

do_test "$MP4BOX -dash 1000 -profile dashavc264:onDemand $TEMP_DIR/file.mp4 -out $TEMP_DIR/file.mpd" "basic-dash-if-ondemand"

do_hash_test $TEMP_DIR/file.mpd "mpd"
do_hash_test $TEMP_DIR/file_dashinit.mp4 "seg"

do_playback_test "$TEMP_DIR/file.mpd" "play-dash-if-ondemand"

test_end
