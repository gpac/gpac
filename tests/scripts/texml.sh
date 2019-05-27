
#@bt_test execute tests on BT file: BT<->XMT, BT<->MP4, XMT<->MP4,  conversions BT, XMT and MP4 Playback
txml_test()
{
 txmlfile=$1
 mp4file="$TEMP_DIR/test.mp4"
 name=$(basename $1)
 name=${name%.*}

 #start our test, specifying all hash names we will check
 test_begin "txml-$name"
 if [ $test_skip  = 1 ] ; then
  return
 fi

 do_test "$MP4BOX -add $txmlfile -new $mp4file" "txml-to-mp4"
 do_hash_test "$mp4file" "txml-to-mp4"

 test_end
}



for i in $EXTERNAL_MEDIA_DIR/texml/* ; do
txml_test $i
done

