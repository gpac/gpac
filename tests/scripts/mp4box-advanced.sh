#!/bin/sh


test_begin "mp4box-writebuf"

if [ $test_skip = 0 ] ; then

 output=$TEMP_DIR/test.mp4
 do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264:dur=1 -write-buffer 20000 -new $output" "import"
 do_hash_test $output "import"

fi

test_end
