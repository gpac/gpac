#!/bin/sh

test_begin "dash-mainprofile-singlefile"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264  -new $TEMP_DIR/file.mp4" "dash-input-preparation"

do_test "$MP4BOX -dash 1000 $TEMP_DIR/file.mp4#video --single-file -profile main -subdur 1000 -dash-ctx $TEMP_DIR/dash_ctx -segment-name "test-\$Number\$" -out $TEMP_DIR/file.mpd" "dash-mainprofile-singlefile-1"

do_test "$MP4BOX -dash 1000 $TEMP_DIR/file.mp4#video --single-file -profile main -subdur 1000 -dash-ctx $TEMP_DIR/dash_ctx -segment-name "test-\$Number\$" -out $TEMP_DIR/file.mpd" "dash-mainprofile-singlefile-2"

do_hash_test $TEMP_DIR/file.mpd "dash-mainprofile-singlefile-hash-mpd"

do_playback_test "$TEMP_DIR/file.mpd" "play-dash-mainprofile-singlefile"

test_end
