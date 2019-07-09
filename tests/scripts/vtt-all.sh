
#@bt_test execute tests on BT file: BT<->XMT, BT<->MP4, XMT<->MP4,  conversions BT, XMT and MP4 Playback
vtt_test ()
{
 vttfile=$1
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

 mp4file="$TEMP_DIR/test.mp4"

 #VTT->MP4
 do_test "$MP4BOX -add $vttfile -new $mp4file" "vtt-to-mp4"
 do_hash_test "$mp4file" "vtt-to-mp4"

 #MP4 with VTT xml dump
if [ $2 = 1 ] ; then
 do_test "$MP4BOX -dxml $mp4file -out $TEMP_DIR/dump.xml" "vtt-dxml"
 do_hash_test "$TEMP_DIR/dump.xml" "vtt-dxml"

 do_test "$MP4BOX -dxml -mergevtt $mp4file -out $TEMP_DIR/dump2.xml" "vtt-merge-dxml"
 do_hash_test "$TEMP_DIR/dump2.xml" "vtt-merge-dxml"

 do_test "$MP4BOX -raw 1 $mp4file -out $TEMP_DIR/dump.vtt" "vtt-dump"
 do_hash_test "$TEMP_DIR/dump.vtt" "vtt-dump"

if [ $keep_temp_dir != 1 ] ; then
 do_test "$MP4BOX -six 1 $mp4file -out $TEMP_DIR/dumpvtt" "six-dump"
 do_hash_test "$TEMP_DIR/dumpvtt.six" "six-dump"
else
 echo "skipping hash, invalid when per-test temp dir is used"
fi

 do_test "$MP4BOX -webvtt-raw 1 $mp4file -out $TEMP_DIR/vttraw" "vttraw-dump"
 do_hash_test "$TEMP_DIR/vttraw.vtt" "vttraw-dump"

fi


# rm $mp4file 2> /dev/null

 test_end
}


vtt_tests ()
{
 first_test=1

 for t in $MEDIA_DIR/webvtt/* ; do
  vtt_test $t $first_test
  first_test=0
 done
}

vtt_tests

