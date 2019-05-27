
oggdump_test ()
{

 name=$(basename $1)
 test_begin "oggdump-$name"

 mp4file="$TEMP_DIR/$name.mp4"
 oggfile="$TEMP_DIR/dump.ogg"

 if [ $test_skip  = 1 ] ; then
  return
 fi

 do_test "$MP4BOX -add $1 -new $mp4file" "add"
 do_hash_test $mp4file "add"

 do_test "$MP4BOX -raw 1 $mp4file -out $oggfile" "dump"
 do_hash_test $oggfile "dump"

 test_end
}


  oggdump_test $EXTERNAL_MEDIA_DIR/import/dead_ogg.ogg
  oggdump_test $EXTERNAL_MEDIA_DIR/import/alsa-6ch.opus

