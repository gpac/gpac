
#@mp4_test execute basics MP4Box tests on source file: -add, -info, -dts, -hint -drtp -sdp -unhint and MP4 Playback
qt_prores_test ()
{

 name=$(basename $1)
 test_begin "qt-prores-$name"

 if [ $test_skip  = 1 ] ; then
  return
 fi
 mp4file="$TEMP_DIR/$name.mp4"
 mp4edit="$TEMP_DIR/qtedit.mp4"

 #test media
 do_test "$MP4BOX -info $1" "RawMediaInfo"

 myinspect=$TEMP_DIR/inspect.txt
 do_test "$GPAC -i $1 inspect:allp:deep:interleave=false:log=$myinspect -graph -stats"
 do_hash_test $myinspect "inspect"


 #import media
 do_test "$MP4BOX -add $1 -no-iod -new $mp4file" "MediaImport"
 do_hash_test $mp4file "add"

 #remove media track media
 do_test "$MP4BOX -rem 1 $mp4file -out $mp4edit" "MediaEdit"
 do_hash_test $mp4edit "rem"

 test_end
}



for src in $EXTERNAL_MEDIA_DIR/qt_prores/* ; do
 qt_prores_test $src
done

