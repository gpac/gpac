#!/bin/sh

test_begin "nhnt"

if [ $test_skip  = 1 ] ; then
return
fi

mp4file="$TEMP_DIR/nhnt.mp4"
do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264 -new $mp4file" "create-isom"
do_test "$MP4BOX -nhnt 1 $mp4file" "create-nhnt"
do_hash_test $TEMP_DIR/nhnt_track1.nhnt "nhnt"
do_hash_test $TEMP_DIR/nhnt_track1.info "nhnt-info"
do_hash_test $TEMP_DIR/nhnt_track1.media "nhnt-media"

test_end
