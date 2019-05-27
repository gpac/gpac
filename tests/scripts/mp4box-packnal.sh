#!/bin/sh


test_begin "mp4box-pack_nal"

if [ $test_skip = 0 ] ; then

src=$MEDIA_DIR/auxiliary_files/enst_video.h264
output=$TEMP_DIR/test.mp4
#we also test setting filter options through :dopt and :sopt
do_test "$MP4BOX -add $src:fstat:fgraph:dopt:pack_nal -new $output" "import"
do_hash_test $output "import"

fi

test_end
