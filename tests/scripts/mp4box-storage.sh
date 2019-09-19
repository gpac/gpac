

test_begin "mp4box-storage"
if [ "$test_skip" != 1 ] ; then

#tested in mp4box-base.sh
#mp4file="$TEMP_DIR/test-flat.mp4"
#do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264 -add $MEDIA_DIR/auxiliary_files/enst_audio.aac -flat -new $mp4file" "flat-mp4"
#do_hash_test $mp4file "flat-mp4"


mp4file="$TEMP_DIR/test-streamable.mp4"
do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264 -add $MEDIA_DIR/auxiliary_files/enst_audio.aac -flat -inter 0 -new $mp4file" "streamable-mp4"
do_hash_test $mp4file "streamable-mp4"

#tested in mp4box-base.sh
#mp4file="$TEMP_DIR/test-interleaved.mp4"
#do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264 -add $MEDIA_DIR/auxiliary_files/enst_audio.aac -inter 1000 -new $mp4file" "interleaved-mp4"
#do_hash_test $mp4file "interleaved-mp4"


mp4file="$TEMP_DIR/test-fastinterleaved.mp4"
do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264 -add $MEDIA_DIR/auxiliary_files/enst_audio.aac -newfs $mp4file" "fast-interleaved-mp4"
#we have no guarantee on the pid ordering in the resulting file, this hash can fail from time to time
do_hash_test $mp4file "fast-interleaved-mp4"


mp4file="$TEMP_DIR/test-gpac-faststart.mp4"
do_test "$GPAC -i $MEDIA_DIR/auxiliary_files/enst_video.h264 -i $MEDIA_DIR/auxiliary_files/enst_audio.aac -o $mp4file:store=fstart" "gpac-faststart-mp4"
#we have no guarantee on the pid ordering in the resulting file, this hash can fail from time to time
do_hash_test $mp4file "faststart-mp4"

fi

test_end



