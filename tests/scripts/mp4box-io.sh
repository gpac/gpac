hint_test ()
{
 tempfile=$1'.tmp'
 hintfile=$1'.hint'
 #hint media
 do_test "$MP4BOX -hint $1 -out $hintfile" "RTPHint"
 do_hash_test $hintfile "hint"

 #check SDP+RTP packets
 do_test "$MP4BOX -drtp $hintfile -out $tempfile" "RTPDump"
 do_hash_test "$tempfile" "drtp"

 #unhint media
 do_test "$MP4BOX -unhint $hintfile" "RTPUnhint"
 do_hash_test $hintfile "unhint"

 rm $tempfile
 rm $hintfile
}

#@mp4_test execute basics MP4Box tests on source file: -add, -info, -dts, -hint -drtp -sdp -unhint and MP4 Playback
mp4_test ()
{
 name=$(basename $1)
 name=${name%.*}
 mp4file="$TEMP_DIR/$name.mp4"
 tmp1="$TEMP_DIR/$name.1.tmp"
 tmp2="$TEMP_DIR/$name.2.tmp"

 do_hint=1

 #ignore xlst & others, no hinting for images
 case $1 in
 *.xslt )
  return ;;
 *.bt )
  return ;;
 *.wrl )
  return ;;
 *.jpg )
  do_hint=0 ;;
 *.jpeg )
  do_hint=0 ;;
 *.png )
  do_hint=0 ;;
 *.mj2 )
  do_hint=0 ;;
 esac

 #only check the logo.png
 if [ $1 = "*/logo.jpg" ] ; then
  return
 fi

 if [ $do_hint != 0 ] ; then
  test_begin "mp4box-io-$name" "add" "diso" "dts" "hint" "drtp" "unhint" "play"
 else
  test_begin "mp4box-io-$name" "add" "diso" "dts" "play"
 fi

 if [ $test_skip  = 1 ] ; then
  return
 fi

 #import media
 do_test "$MP4BOX -add $1 -new $mp4file" "MediaImport"
 do_hash_test $mp4file "add"

 #all the tests below are run in parallel

 #test info - no hash on this one we usually change it a lot
 do_test "$MP4BOX -info $mp4file" "MediaInfo" &

 #test -diso
 do_test "$MP4BOX -diso $mp4file -out $tmp1" "MediaInfo" && do_hash_test $tmp1 "diso" && rm $tmp1 &

 #test dts
 do_test "$MP4BOX -dts $mp4file -out $tmp2" "MediaTime" && do_hash_test $tmp2 "dts" && rm $tmp2 &


 if [ $do_hint != 0 ] ; then
  hint_test $mp4file &
 fi

 #MP4 playback
 do_playback_test $mp4file "play" && rm $mp4file &

 test_end
}


mp4box_tests ()
{

 for src in $MEDIA_DIR/auxiliary_files/* ; do
  mp4_test $src
 done

 if [ $EXTERNAL_MEDIA_AVAILABLE = 0 ] ; then
  return
 fi

 for src in $EXTERNAL_MEDIA_DIR/import/* ; do
  mp4_test $src
 done

}

mp4box_tests
#mp4_test "auxiliary_files/enst_audio.aac"
#mp4_test $MEDIA_DIR/import/speedway.mj2

