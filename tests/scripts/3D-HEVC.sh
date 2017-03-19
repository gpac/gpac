#!/bin/sh
 if [ $EXTERNAL_MEDIA_AVAILABLE = 0 ] ; then
  return
 fi


test_begin "3D-HEVC"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/3D-HEVC/stream_bbb.bit:fmt=HEVC -new $TEMP_DIR/test.mp4" "import"

do_hash_test $TEMP_DIR/test.mp4 "import"

do_playback_test "$TEMP_DIR/test.mp4" "play"

test_end


test_begin "add-subsamples-HEVC"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/3D-HEVC/stream_bbb.bit:fmt=HEVC:subsamples -new $TEMP_DIR/test.mp4" "add-subsamples-HEVC"
do_hash_test $TEMP_DIR/test.mp4 "import"

test_end
