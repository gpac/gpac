#!/bin/sh
test_begin "no-frag-default"
 if [ $test_skip  = 1 ] ; then
  return
 fi

$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264 -new $TEMP_DIR/test.mp4 2> /dev/null

do_test "$MP4BOX -dash 1000 -rap -no-frags-default -single-file -out $TEMP_DIR/test.mpd $TEMP_DIR/test.mp4" "dash"

do_hash_test "$TEMP_DIR/test_dashinit.mp4" "dash"

do_playback_test "$TEMP_DIR/test_dashinit.mp4" "play"

test_end
