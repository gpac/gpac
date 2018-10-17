#!/bin/sh

rm -f "$TEMP_DIR/file.mpd"
rm -f "$TEMP_DIR/dash_ctx"

test_begin "dash-timeline"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264 -new $TEMP_DIR/file.mp4" "dash-input-preparation"

do_test "$MP4BOX -dash 1000 $TEMP_DIR/file.mp4:bandwidth=600000 -subdur 1000 -dash-ctx $TEMP_DIR/dash_ctx -segment-timeline -profile live -out $TEMP_DIR/file.mpd" "dash-timeline-dash1"

do_hash_test $TEMP_DIR/file.mpd "dash-template-hash-mpd1"

#do another pass to check segment timeline store/restore from context
do_test "$MP4BOX -dash 1000 $TEMP_DIR/file.mp4:bandwidth=600000 -subdur 1000 -dash-ctx $TEMP_DIR/dash_ctx -segment-timeline -profile live -out $TEMP_DIR/file2.mpd" "dash-timeline-dash2"

do_hash_test $TEMP_DIR/file2.mpd "dash-template-hash-mpd2"

test_end
