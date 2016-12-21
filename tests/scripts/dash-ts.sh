#!/bin/sh

test_begin "dash-ts"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter-30s_I25_baseline_1280x720_512kbps.264 -add $EXTERNAL_MEDIA_DIR/counter/counter-30s_audio.aac -new $TEMP_DIR/file.mp4" "ts-for-dash-input-preparation"

do_test "$MP42TS -src $TEMP_DIR/file.mp4 -dst-file=$TEMP_DIR/file.ts" "ts-for-dash-input-preparation-2"

do_test "$MP4BOX -dash 1000 $TEMP_DIR/file.ts -out $TEMP_DIR/file.mpd" "ts-dash"

do_playback_test "$TEMP_DIR/file.mpd" "play-ts-dash"

test_end
