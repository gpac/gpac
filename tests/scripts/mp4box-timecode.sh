#!/bin/sh

test_begin "mp4box-timecode"

 if [ $test_skip  = 1 ] ; then
  return
 fi

do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264:rescale=24000/1001:tc=24000/1001,0,10,10,2 -new $TEMP_DIR/file.mp4" "rescale_tmcd"
do_hash_test "$TEMP_DIR/file.mp4" "rescale_tmcd"

do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264:rescale=30000/1001:tc=f30000/1001,0,10,10,2 -new $TEMP_DIR/file_f.mp4" "rescale_tmcd_f"
do_hash_test "$TEMP_DIR/file_f.mp4" "rescale_tmcd_f"

test_end
