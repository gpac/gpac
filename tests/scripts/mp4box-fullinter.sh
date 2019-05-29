

test_begin "mp4box-hint-tight"
if [ "$test_skip" = 1 ] ; then
return
fi

mp4file="$TEMP_DIR/udtamoov.mp4"

do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_audio.aac -add $MEDIA_DIR/auxiliary_files/enst_video.h264 -hint -tight -new $mp4file" "hint-tight"
do_hash_test $mp4file "hint-tight"

test_end


