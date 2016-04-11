#test MP4Box RTP streaming and MP4 client
test_begin "mp4box-rtp" "play"
if [ $test_skip  = 1 ] ; then
return
fi

mp4file="$TEMP_DIR/test.mp4"
do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264:dur=2 -add $MEDIA_DIR/auxiliary_files/enst_audio.aac:dur=2 -new $mp4file" "build-mp4"

do_test "$MP4BOX -rtp -run-for=0 -sdp=$TEMP_DIR/session.sdp -dst=239.255.0.1 $mp4file" "streamer-init"

#we have a bug in avi dumping of RTP source, just check regular playback for now
do_test "$MP4CLIENT -run-for 5 $TEMP_DIR/session.sdp" "play" &

#run for a bit more because of buffering
do_test "$MP4BOX -rtp -run-for=10 -sdp=$TEMP_DIR/session.sdp -dst=239.255.0.1 $mp4file" "streamer-run"

test_end
