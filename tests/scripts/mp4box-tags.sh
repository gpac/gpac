

test_begin "mp4box-tags"
if [ "$test_skip" = 1 ] ; then
return
fi

mp4file="$TEMP_DIR/test.mp4"
do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_audio.aac -new $mp4file -itags cover=$MEDIA_DIR/auxiliary_files/logo.png" "create"
do_hash_test $mp4file "create"

covfile="$TEMP_DIR/test.png"
do_test "$MP4BOX -dump-cover $mp4file -out $covfile" "dumpcover"
do_hash_test $covfile "dumpcover"

test_end

