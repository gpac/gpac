
#@bt_test execute tests on BT file: BT<->XMT, BT<->MP4, XMT<->MP4,  conversions BT, XMT and MP4 Playback
vtt_test ()
{
 vttfile=$1
 mp4file="$TEMP_DIR/test.mp4"
 name=$(basename $1)
 name=${name%.*}

 case $1 in
 *.srt )
  name="$name-srt" ;;
 esac

 #start our test, specifying all hash names we will check
 test_begin "vtt-$name"
 if [ $test_skip  = 1 ] ; then
  return
 fi

 #BT->XMT
 do_test "$MP4BOX -add $vttfile -new $mp4file" "vtt-to-mp4"
 do_hash_test "$mp4file" "vtt-to-mp4"
 do_test "$MP4BOX -info 1 $mp4file" "vtt-info"

#FIXME
#do_test "$GPAC -i $mp4file compositor:osize=512x192:vfr:dur=10 @ -o $TEMP_DIR/dump.rgb" "play"
#do_hash_test "$TEMP_DIR/dump.rgb" "play"
#do_play_test "play" "$TEMP_DIR/dump.rgb:size=512x192" ""



 rm $mp4file 2> /dev/null

 test_end
}


vtt_tests ()
{
 for t in $MEDIA_DIR/webvtt/* ; do
  vtt_test $t
 done
}

vtt_tests

