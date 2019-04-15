#!/bin/sh

rm -f "$TEMP_DIR/file.mpd"
rm -f "$TEMP_DIR/dash_ctx"

test_begin "dash-mainprofile-singlefile"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264  -new $TEMP_DIR/file.mp4" "dash-input-preparation"

do_test "$MP4BOX -dash 1000 $TEMP_DIR/file.mp4#video -single-file -profile main -subdur 1000 -dash-ctx $TEMP_DIR/dash_ctx -segment-name "test-\$Number\$" -out $TEMP_DIR/file.mpd" "dash-mainprofile-singlefile-1"

do_hash_test $TEMP_DIR/file.mpd "mpd"
do_hash_test $TEMP_DIR/test-init.mp4 "seg"


myinspect=$TEMP_DIR/inspect.txt
do_test "$GPAC -i $TEMP_DIR/file.mpd inspect:all:deep:interleave=false:dur=2/1:log=$myinspect"
do_hash_test $myinspect "inspect"

test_end
