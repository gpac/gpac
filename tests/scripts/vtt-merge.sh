
 test_begin "vtt-merge"
 if [ $test_skip  = 1 ] ; then
  return
 fi

 mp4file=$TEMP_DIR/file.mp4
 vttfile=$TEMP_DIR/file.vtt
 do_test "$MP4BOX -add $MEDIA_DIR/webvtt/overlapping.vtt -new $mp4file" "vtt-to-mp4"
 do_test "$GPAC -i $mp4file -o $vttfile:merge" "vtt-merge"
 do_hash_test "$vttfile" "vtt-merge"

 test_end

