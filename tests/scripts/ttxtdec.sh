ttxt_test()
{
 test_begin "ttxtdec-$1"

 if [ $test_skip  = 1 ] ; then
  return
 fi

 srcfile=$2

 if [ $3 == 1 ] ; then
 srcfile=$TEMP_DIR/test.mp4
 $MP4BOX -add $2 -new $srcfile 2> /dev/null
 fi

 dump=$TEMP_DIR/dump.rgb

 #test source parsing and playback
 do_test "$GPAC -font-dirs=$EXTERNAL_MEDIA_DIR/fonts/ -rescan-fonts -i $srcfile compositor:osize=512x128:vfr @ -o $dump" "srcplay"
 #don't hash content on 32 bits, fp precision leads to different results
 if [ $GPAC_OSTYPE != "lin32" ] ; then
  do_hash_test $dump "srcplay"
 fi

 do_play_test "dump" "$dump:size=512x128"

 test_end
}


#test srt
ttxt_test "srt" $MEDIA_DIR/auxiliary_files/subtitle.srt 0

#test ttxt
ttxt_test "ttxt" $MEDIA_DIR/auxiliary_files/subtitle.ttxt 0

#test ttxt
ttxt_test "tx3g" $MEDIA_DIR/auxiliary_files/subtitle.srt 1

#test webvtt
ttxt_test "vtt" $MEDIA_DIR/webvtt/simple.vtt 0

