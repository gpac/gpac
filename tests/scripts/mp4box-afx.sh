#!/bin/sh


test_begin "mp4box-afx"

if [ $test_skip = 0 ] ; then

src=$TEMP_DIR/file.s3d
cp $MEDIA_DIR/auxiliary_files/logo.jpg $src
output=$TEMP_DIR/test.mp4
do_test "$MP4BOX -add $src -new $output" "import"
do_hash_test $output "import"

fi

test_end
