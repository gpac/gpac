#!/bin/sh

test_begin "dash-mainprofile"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264  -new $TEMP_DIR/file.mp4" "dash-input-preparation"

do_test "$MP4BOX -dash 1000 $TEMP_DIR/file.mp4#video -dash-ctx $TEMP_DIR/dash_ctx -profile main -subdur 1000 -segment-name "test-\$Number\$" -out $TEMP_DIR/file.mpd" "dash-mainprofile-1"

do_test "$MP4BOX -dash 1000 $TEMP_DIR/file.mp4#video -dash-ctx $TEMP_DIR/dash_ctx -profile main -subdur 1000 -segment-name "test-\$Number\$" -out $TEMP_DIR/file.mpd" "dash-mainprofile-2"

do_hash_test $TEMP_DIR/file.mpd "dash-mainprofile-hash-mpd"

do_playback_test "$TEMP_DIR/file.mpd" "play-mainprofile"

test_end
