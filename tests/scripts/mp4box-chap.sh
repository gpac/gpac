

test_begin "mp4box-chap"
if [ "$test_skip" = 1 ] ; then
return
fi

mp4file="$TEMP_DIR/base.mp4"

do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/count_video.cmp -add $MEDIA_DIR/auxiliary_files/subtitle.srt:chap -new $mp4file" "chap"

do_hash_test $mp4file "chap"

test_end


