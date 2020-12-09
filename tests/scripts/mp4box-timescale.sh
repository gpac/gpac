#!/bin/sh

test_timescale()
{
test_begin "mp4box-timescale-$1"

 if [ $test_skip  = 1 ] ; then
  return
 fi

output=$TEMP_DIR/test.mp4
do_test "$MP4BOX -add $2:moovts=-1 -new $output" "moovts-from-raw"
do_hash_test $output "moovts-from-raw"

input=$TEMP_DIR/src.mp4
$MP4BOX -add $2 -new $input 2> /dev/null

output=$TEMP_DIR/test2.mp4
do_test "$MP4BOX -add $input:moovts=-1 -new $output" "moovts-from-mp4"
do_hash_test $output "moovts-from-mp4"

do_test "$MP4BOX -dash 1000 $input:sscale -bound -profile onDemand -out $TEMP_DIR/test.mpd" "dash-sscale"
do_hash_test $TEMP_DIR/src_dashinit.mp4 "dash-sscale-init"
#do_hash_test $TEMP_DIR/test.mpd "dash-sscale-mpd"

test_end

}

test_timescale "aac" "$MEDIA_DIR/auxiliary_files/enst_audio.aac"

test_timescale "avc" "$MEDIA_DIR/auxiliary_files/enst_video.h264"
