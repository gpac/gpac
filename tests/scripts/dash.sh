#!/bin/sh

test_begin "dash"

do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264 -add $MEDIA_DIR/auxiliary_files/enst_audio.aac -new $TEMP_DIR/file.mp4" "dash-input-preparation"

do_test "$MP42TS -src $TEMP_DIR/file.mp4 -dst-file=$TEMP_DIR/file.ts" "ts-for-dash-input-preparation"

do_test "$MP4BOX -dash 1000 $TEMP_DIR/file.mp4 -out $TEMP_DIR/file.mpd" "basic-dash"

do_test "$MP4BOX -dash 1000 $TEMP_DIR/file.ts -out $TEMP_DIR/file2.mpd" "ts-dash"

do_test "$MP4BOX -mpd-refresh 1000 -dash-ctx $TEMP_DIR/dash_ctx -dash-run-for 20000 -subdur 100 -dynamic -profile live -dash-live 100 $TEMP_DIR/file.mp4 -out $TEMP_DIR/file3.mpd" "dash-live"

do_playback_test "$TEMP_DIR/file.mpd" "play"

test_end
