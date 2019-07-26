


test_begin "mp4box-misc"
if [ "$test_skip" = 1 ] ; then
return
fi

mp4file="$TEMP_DIR/hdr.mp4"
do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264 -hdr $MEDIA_DIR/auxiliary_files/hdr.xml -new $mp4file" "hdr"
do_hash_test "$mp4file" "hdr"

test_end

