hint_test ()
{
 tempfile=$1'.tmp'
 hintfile=$1'.hint'
 #hint media
 do_test "$MP4BOX -hint $1 -out $hintfile" "RTPHint"
if [ $do_hash != 0 ] ; then
 do_hash_test $hintfile "hint"
fi

 #check SDP+RTP packets
 do_test "$MP4BOX -drtp $hintfile -out $tempfile" "RTPDump"
if [ $do_hash != 0 ] ; then
 do_hash_test "$tempfile" "drtp"
fi

 #unhint media
 do_test "$MP4BOX -unhint $hintfile" "RTPUnhint"
if [ $do_hash != 0 ] ; then
 do_hash_test $hintfile "unhint"
fi

 rm $tempfile
 rm $hintfile
}

#@mp4_test execute basics MP4Box tests on source file: -add, -info, -dts, -hint -drtp -sdp -unhint and MP4 Playback
mp4_test ()
{
 do_hint=1
 do_play=1
 do_hash=1

 #ignore xlst & others, no hinting for images
 case $1 in
 *.xslt )
  return ;;
 *.ttxt )
  return ;;
 *.opus )
  return ;;
 *.html* )
  return ;;
 *.bt )
  return ;;
 *.wrl )
  return ;;
 #only check the logo.png
 */logo.jpg )
  return ;;
 # mkv not supported yet
 *.mkv )
  return ;;
 *.jpg )
  do_hint=0 ;;
 *.jpeg )
  do_hint=0 ;;
 *.jp2 )
  do_hint=0 ;;
 *.mj2 )
  do_hint=0 ;;
 *.av1 )
  do_hint=0 ;;
 *.opus )
  do_hint=0 ;;
 *.obu )
  do_hint=0 ;;
 *.ivf )
  do_hint=0 ;;
 *.png )
  do_hint=0 ;;
 *.qcp )
  do_play=0 ;;
  #mpg, ogg and avi import is broken in master, disable hash until we move to filters
  #two many diffs in SVC import (sei, svc subseq PS) between old and new archs, we comment out hashing for now
 *.mpg | *.ogg | *.avi | *svc* )
  do_hash=0 ;;
 #no support for hinting or playback yet
 *.ismt )
  do_hint=0 && do_play=0 ;;
 esac

 name=$(basename $1)
 test_begin "mp4box-io-$name"

 mp4file="$TEMP_DIR/$name.mp4"
 tmp1="$TEMP_DIR/$name.1.tmp"
 tmp2="$TEMP_DIR/$name.2.tmp"

 if [ $test_skip  = 1 ] ; then
  return
 fi

 #test media
 do_test "$MP4BOX -info $1" "RawMediaInfo"
 #import media
 do_test "$MP4BOX -add $1 -new $mp4file" "MediaImport"

 if [ $do_hash != 0 ] ; then
  do_hash_test $mp4file "add"
 fi

 #all the tests below are run in parallel

 #test info - no hash on this one we usually change it a lot
 do_test "$MP4BOX -info $mp4file" "MediaInfo" &

 #test -diso
 do_test "$MP4BOX -diso $mp4file -out $tmp1" "XMDDump"
if [ $do_hash != 0 ] ; then
 do_hash_test $tmp1 "diso"
fi
 rm $tmp1 2> /dev/null &

 #test dts
 do_test "$MP4BOX -dts $mp4file -out $tmp2" "MediaTime"
if [ $do_hash != 0 ] ; then
 do_hash_test $tmp2 "dts"
fi
 rm $tmp2 2> /dev/null &


 if [ $do_hint != 0 ] ; then
  hint_test $mp4file &
 fi

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

