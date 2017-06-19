#!/bin/sh

test_begin "dash-template"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264 -new $TEMP_DIR/file.mp4" "dash-input-preparation"

do_test "$MP4BOX -dash 1000 $TEMP_DIR/file.mp4:bandwidth=600000 -profile live -segment-name test-\$Bandwidth\$-\$Time%05d\$\$Init=is\$  -out $TEMP_DIR/file.mpd" "dash-template-dash"

do_hash_test $TEMP_DIR/file.mpd "dash-template-hash-mpd"

do_hash_test $TEMP_DIR/test-600000-is.mp4 "dash-template-init-segment-audio"

do_playback_test "$TEMP_DIR/file.mpd" "play-dash-template"

test_end
