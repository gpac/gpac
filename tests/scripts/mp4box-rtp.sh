return

#not done yet

#test MP4Box RTP streaming and MP4 client
test_begin "mp4box-rtp" "play"
if [ $test_skip  = 1 ] ; then
return
fi

mp4file="$TEMP_DIR/test.mp4"
do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264 -add $MEDIA_DIR/auxiliary_files/enst_audio.aac -new $mp4file"

do_test "$MP4BOX -rtp -sdp=$TEMP_DIR/session.sdp $mp4file"
do_playback_test "-guid -stats $TEMP_DIR/session.sdp" "MP4ClientRTP"

test_end
