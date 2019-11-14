#!/bin/sh

test_begin "mp4box-timecode"

 if [ $test_skip  = 1 ] ; then
  return
 fi

do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264:rescale=24000/1001:tc=24000/1001,0,10,10,2 -new $TEMP_DIR/file.mp4" "rescale_tmcd"
do_hash_test "$TEMP_DIR/file.mp4" "rescale_tmcd"

do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264:rescale=30000/1001:tc=30000/1001,0,10,10,2,30 -new $TEMP_DIR/file_fpt.mp4" "rescale_tmcd_fpt"
do_hash_test "$TEMP_DIR/file_fpt.mp4" "rescale_tmcd_fpt"

test_end
