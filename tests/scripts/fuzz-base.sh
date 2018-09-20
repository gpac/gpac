
#@mp4_test execute basics MP4Box tests on source file: -add, -info, -dts, -hint -drtp -sdp -unhint and MP4 Playback
fuzz_encode ()
{
 name=$(basename $1)
 mp4file="$TEMP_DIR/$name.mp4"

 test_begin "fuzz-base-$name"

 if [ $test_skip  = 1 ] ; then
  return
 fi

 fuzz_test=1

 #encode media
 do_test "$MP4BOX -mp4 $1 -out $mp4file" "MediaEncode"

 rm $mp4file
 test_end
}

#@mp4_test execute basics MP4Box tests on source file: -add, -info, -dts, -hint -drtp -sdp -unhint and MP4 Playback
fuzz_import ()
{
 name=$(basename $1)
 mp4file="$TEMP_DIR/$name.mp4"

 test_begin "fuzz-base-$name"

 if [ $test_skip  = 1 ] ; then
  return
 fi

 fuzz_test=1

 #test media
 do_test "$MP4BOX -info $1" "RawMediaInfo"
 #import media
 do_test "$MP4BOX -add $1 -new $mp4file" "MediaImport"

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


