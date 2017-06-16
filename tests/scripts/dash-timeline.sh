#!/bin/sh

test_begin "dash-timeline"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264 -new $TEMP_DIR/file.mp4" "dash-input-preparation"

do_test "$MP4BOX -dash 1000 $TEMP_DIR/file.mp4:bandwidth=600000 -segment-timeline -profile live -out $TEMP_DIR/file.mpd" "basic-dash"

do_hash_test $TEMP_DIR/file.mpd "dash-template-hash-mpd"

# TO DO: same with context !!!!
#do_playback_test "$TEMP_DIR/file.mpd" "play-basic-dash"

test_end
