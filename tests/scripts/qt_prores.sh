
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

 #import media
 do_test "$MP4BOX -add $1:asemode=v1-qt -no-iod -new $mp4file" "MediaImport"
 do_hash_test $mp4file "add"

 #remove media track media
 do_test "$MP4BOX -rem 1 $mp4file -out $mp4edit" "MediaEdit"
 do_hash_test $mp4edit "rem"

 #import media as mov, force bit depth to 32 and set color profile
 movfile="$TEMP_DIR/$name.mov"
 do_test "$MP4BOX -add $1:bitdepth=32:colr=nclc,1,1,1 -new $movfile" "MediaImportMoov"
 do_hash_test $movfile "addmov"

 test_end
}



for src in $EXTERNAL_MEDIA_DIR/qt_prores/* ; do
 qt_prores_test $src
done

