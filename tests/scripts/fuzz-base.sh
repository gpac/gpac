
#@mp4_test execute basics MP4Box tests on source file: -add, -info, -dts, -hint -drtp -sdp -unhint and MP4 Playback
fuzz_encode ()
{
 name=$(basename $1)
 mp4file="$TEMP_DIR/$name.mp4"
 mp4hint="$TEMP_DIR/$name.hint.tmp"

 test_begin "fuzz-base-$name"

 if [ $test_skip  = 1 ] ; then
  return
 fi

 fuzz_test=1

 #encode media
 do_test "$MP4BOX -mp4 $1 -out $mp4file" "MediaEncode"

 #hint media
 do_test "$MP4BOX -hint $mp4file -out $mp4hint" "MediaHint"
 rm $mp4hint

 #MP4 playback for 1 sec only (too long otherwise)
 do_test "$MP4CLIENT -run-for 0 $mp4file" "MediaPlay"

 rm $mp4file
 test_end
}

#@mp4_test execute basics MP4Box tests on source file: -add, -info, -dts, -hint -drtp -sdp -unhint and MP4 Playback
fuzz_import ()
{
 do_hint=1
 do_play=1

 #ignore xlst & others, no hinting for images
 case $1 in
 *.jpg )
  do_hint=0 ;;
 *.jpeg )
  do_hint=0 ;;
 *.png )
  do_hint=0 ;;
 *.mj2 )
  do_hint=0 ;;
 *.qcp )
  do_play=0 ;;
 #no support for hinting or playback yet
 *.ismt )
  do_hint=0 && do_play=0 ;;
 esac

 name=$(basename $1)
 mp4file="$TEMP_DIR/$name.mp4"
 mp4hint="$TEMP_DIR/$name.hint.tmp"

 test_begin "fuzz-base-$name"

 if [ $test_skip  = 1 ] ; then
  return
 fi

 fuzz_test=1

 #test media
 do_test "$MP4BOX -info $1" "RawMediaInfo"
 #import media
 do_test "$MP4BOX -add $1 -new $mp4file" "MediaImport"

 if [ $do_hint != 0 ] ; then
  #hint media
  do_test "$MP4BOX -hint $mp4file -out $mp4hint" "MediaHint"
  rm $mp4hint
 fi

 if [ $do_play != 0 ] ; then
  #MP4 playback for 1 sec only (too long otherwise)
  do_test "$MP4CLIENT -run-for 0 $mp4file" "MediaPlay"
 fi

 rm $mp4file
 test_end
}


if [ $EXTERNAL_MEDIA_AVAILABLE = 0 ] ; then
  return
fi

for src in $EXTERNAL_MEDIA_DIR/fuzzing/* ; do
 case $src in 
 *.xmt )
 fuzz_encode $src
 ;;
 *.bt )
 fuzz_encode $src
 ;;
 *.xsr )
 fuzz_encode $src
 ;;
 *.info )
 ;;
 *.media )
 ;;
 *.html* )
 ;;
 * )
 fuzz_import $src
 ;;
 esac
done


