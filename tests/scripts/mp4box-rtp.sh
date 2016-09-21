#test MP4Box RTP streaming and MP4 client

#DST="-dst=239.255.0.1"
DST=""

IFCE="127.0.0.1"

rtp_test ()
{

test_begin "mp4box-rtp-$1" 
if [ $test_skip  = 1 ] ; then
return
fi

do_test "$MP4BOX -rtp -run-for=0 -sdp=$TEMP_DIR/session.sdp $DST -ifce=$IFCE $2" "streamer-init"
do_hash_test "$TEMP_DIR/session.sdp" "streamer-init"

#we have a bug in avi dumping of RTP source, just check regular playback for now
do_test "$MP4CLIENT -run-for 2 -opt Network:BufferLength=1000 -no-save -ifce $IFCE $TEMP_DIR/session.sdp" "play" &

#run for a bit more because of buffering and client startup time
do_test "$MP4BOX -rtp -run-for=6 -sdp=$TEMP_DIR/session.sdp $DST -ifce=$IFCE $2" "streamer-run"

test_end
}

mp4file="$TEMP_DIR/test.mp4"

$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264:dur=1 -add $MEDIA_DIR/auxiliary_files/enst_audio.aac:dur=1 -new $mp4file 2> /dev/null
rtp_test "avc-aac" $mp4file


$MP4BOX -add $MEDIA_DIR/auxiliary_files/count_video.cmp:dur=1 -add $MEDIA_DIR/auxiliary_files/count_english.mp3:dur=1 -new $mp4file 2> /dev/null
rtp_test "mpeg4p2-mp3" $mp4file


if [ $EXTERNAL_MEDIA_AVAILABLE = 0 ] ; then
return
fi

$MP4BOX -add $EXTERNAL_MEDIA_DIR/import/bear_video.263:dur=1 -add $EXTERNAL_MEDIA_DIR/import/bear_audio.amr:dur=1 -new $mp4file 2> /dev/null
rtp_test "h263-amr" $mp4file

