#!/bin/sh
test_begin "swf"
 if [ $test_skip  = 1 ] ; then
  return
 fi

do_test "$MP4BOX -mp4 $MEDIA_DIR/swf/cuisine.swf -out $TEMP_DIR/cuisine.mp4" "mp4box-import"
do_hash_test "$TEMP_DIR/cuisine.mp4" "mp4box-import"

do_test "$GPAC -i $TEMP_DIR/cuisine.mp4 compositor:osize=192x192 @ -o $TEMP_DIR/dump.rgb" "play"
do_hash_test "$TEMP_DIR/dump.rgb" "play"

do_play_test "play" "$TEMP_DIR/dump.rgb:size=192x192" ""

test_end
