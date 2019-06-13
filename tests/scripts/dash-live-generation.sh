#!/bin/sh

test_begin "dash-live-generation"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264:dur=5 -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.aac:dur=5 -new $TEMP_DIR/file.mp4" "dash-input-preparation"

do_test "$MP4BOX -bs-switching single -mpd-refresh 10 -time-shift -1 -dash-ctx $TEMP_DIR/dash_ctx -run-for 4000 -dynamic -profile live -dash-live 2000  $TEMP_DIR/file.mp4#video $TEMP_DIR/file.mp4#audio -out $TEMP_DIR/file.mpd" "dash-live" &

sleep 1

myinspect=$TEMP_DIR/inspect.txt
do_test "$GPAC -i $TEMP_DIR/file.mpd inspect:allp:deep:interleave=false:dur=2/1:log=$myinspect"
do_hash_test $myinspect "inspect"

test_end

