#!/bin/sh

if [ $EXTERNAL_MEDIA_AVAILABLE = 0 ] ; then
  return
fi

srcfile=$EXTERNAL_MEDIA_DIR/import/Chimera-AV1-8bit-480x270-552kbps.ivf

test_begin "auxv"

 if [ $test_skip  = 1 ] ; then
  return
 fi

dstfile="$TEMP_DIR/auxv.mp4"
do_test "$MP4BOX -add $srcfile:hdlr=auxv:alpha -new $dstfile" "auxv-track"
do_hash_test $dstfile "auxv-track"

dstfile="$TEMP_DIR/auxv.heif"
do_test "$MP4BOX -add-image $srcfile:hdlr=auxv:alpha -new $dstfile" "auxv-image"
do_hash_test $dstfile "auxv-image"

test_end
