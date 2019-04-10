#!/bin/sh

test_begin "dash-subt"

do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/subtitle.srt -new $TEMP_DIR/file.mp4" "dash-input-preparation"

do_test "$MP4BOX -dash 1500 $TEMP_DIR/file.mp4 -profile live -out $TEMP_DIR/file.mpd" "dash-subt"

do_hash_test $TEMP_DIR/file.mpd "dash-subt-hash-mpd"

myinspect=$TEMP_DIR/inspect.txt
do_test "$GPAC -i $TEMP_DIR/file.mpd inspect:all:deep:interleave=false:log=$myinspect"
do_hash_test $myinspect "inspect"


test_end
