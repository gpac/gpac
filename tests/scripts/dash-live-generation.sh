#!/bin/sh

test_begin "dash-live-generation"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264:dur=5 -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.aac:dur=5 -new $TEMP_DIR/file.mp4" "dash-input-preparation"

do_test "$MP4BOX -bs-switching single -mpd-refresh 10 -dash-ctx $TEMP_DIR/dash_ctx -dash-run-for 10000 -dynamic -profile live -dash-live 2000  $TEMP_DIR/file.mp4#video $TEMP_DIR/file.mp4#audio -out $TEMP_DIR/file.mpd" "dash-live" &

sleep 1

do_playback_test "$TEMP_DIR/file.mpd" "play-basic-dash"

test_end

