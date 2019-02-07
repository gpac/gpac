deps_test ()
{
 name=$(basename $1)

 test_begin "mp4box-deps-$name"
 if [ $test_skip  = 1 ] ; then
  return
 fi

 mp4file="$TEMP_DIR/$name.mp4"

 #import media
 do_test "$MP4BOX -add $1:deps -new $mp4file" "deps-import"
 do_hash_test $mp4file "deps-import"

 mp4file=$TEMP_DIR/$name"_thin.mp4"
 #import and thin media
 do_test "$MP4BOX -add $1:deps:refs -new $mp4file" "deps-thin-import"
 do_hash_test $mp4file "deps-thin-import"
 test_end
}


#avc test
deps_test $MEDIA_DIR/auxiliary_files/enst_video.h264
#test not usefull, video doesn't have B slices so no frames are marked as discardable. We would need further ref pic list inspection for that
#deps_test $MEDIA_DIR/auxiliary_files/counter.hvc

 if [ $EXTERNAL_MEDIA_AVAILABLE = 0 ] ; then
  return
 fi


deps_test $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_640x360_192kbps.264
deps_test $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_openGOP_640x360_160kbps.264

