#!/bin/sh
test_begin "File-with-no-default-flags-in-fragments"

do_test "$MP4BOX -dash 1000 -segment-name mysegs_%s -url-template -no-frags-default -single-file -out $TEMP_DIR/counter-bifs-10sec-bipbop_420_avc.mpd $EXTERNAL_MEDIA_DIR/noFragsDefault/counter-bifs-10sec-160x102-bipbop_420_avc.mp4 $EXTERNAL_MEDIA_DIR/noFragsDefault/counter-bifs-10sec-320x180-bipbop_420_avc.mp4" "mp4box-segmentation-noFragsDefault"
do_hash_test $TEMP_DIR/test.mp4 "mp4box-segmentation-noFragsDefault"

do_playback_test "$TEMP_DIR/counter-bifs-10sec-bipbop_420_avc.mpd" "mp4client-play"

test_end
