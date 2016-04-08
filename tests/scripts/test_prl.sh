#test file for parallel execution of tests, disabled by default
return

my_test ()
{
#test MP4Box RTP streaming and MP4 client
test_begin $2

do_test "$1" $2

test_end

}


mp4file="$TEMP_DIR/test.mp4"
$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264 -add $MEDIA_DIR/auxiliary_files/enst_audio.aac -new $mp4file 2> /dev/null


my_test "$MP4BOX -rtp -run-for=1 -sdp=$TEMP_DIR/session.sdp $mp4file" "testmp4box-rtp" &
my_test "$MP4BOX -frag 500 $mp4file -out $TEMP_DIR/frag.mp4" "testmp4box-frag" &
my_test "$MP4BOX -hint $mp4file -out $TEMP_DIR/hint.mp4" "testmp4box-hint" &
my_test "$MP4BOX -info $mp4file" "testmp4box-info" &

test_begin "testmp4box-multiline" "raw-264"
do_test "$MP4BOX  -raw 1 $mp4file -out $TEMP_DIR/test.tmp" "raw-264" && do_hash_test $TEMP_DIR/test.tmp "raw-264" &
test_end

