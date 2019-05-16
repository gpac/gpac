#!/bin/sh

test_begin "NHMLSubsComp"
 if [ $test_skip  = 1 ] ; then
  return
 fi

mp4file="$TEMP_DIR/file.mp4"
do_test "$MP4BOX -add $MEDIA_DIR/xmlin4/text-stxt-compress.nhml -new $mp4file" "create"
do_hash_test $mp4file "create"

test_end
