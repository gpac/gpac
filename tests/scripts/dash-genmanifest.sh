#!/bin/sh

test_begin "dash-gen-manifest"

 if [ $test_skip  = 1 ] ; then
  return
 fi

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_640x360_192kbps.264 -new $TEMP_DIR/source.mp4" "import"

do_test "$MP4BOX -dash 2000 -profile onDemand $TEMP_DIR/source.mp4 -out $TEMP_DIR/test.mpd" "dash"

do_test "$GPAC -i $TEMP_DIR/source_dashinit.mp4 -o $TEMP_DIR/gen-vod.mpd:sigfrag:profile=onDemand" "gen-ondemand"
do_hash_test "$TEMP_DIR/gen-vod.mpd" "gen-ondemand"

do_test "$GPAC -i $TEMP_DIR/source_dashinit.mp4 -o $TEMP_DIR/gen-main.mpd:sigfrag:profile=main" "gen-main"
do_hash_test "$TEMP_DIR/gen-main.mpd" "gen-main"

do_test "$GPAC -i $TEMP_DIR/source_dashinit.mp4 -o $TEMP_DIR/gen-hls.m3u8:sigfrag" "gen-hls"
do_hash_test "$TEMP_DIR/gen-hls.m3u8" "gen-hls-master"
do_hash_test "$TEMP_DIR/gen-hls_1.m3u8" "gen-hls-subpl"

test_end

