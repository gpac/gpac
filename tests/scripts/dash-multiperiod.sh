test_begin "dash-multiperiod"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_320x180_128kbps.264:dur=5 -new $TEMP_DIR/counter_video_180.mp4" "multiperiod-input-preparation_180"
do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.aac:dur=5 -new $TEMP_DIR/counter_audio.mp4" "multiperiod-input-preparation_audio"

do_test "$MP4BOX -bs-switching no -dash 1000 -dash-profile live \
-out $TEMP_DIR/file.mpd -segment-name \$RepresentationID\$-\$Number\$ \
$TEMP_DIR/counter_audio.mp4:id=ap1:Period=P1 $TEMP_DIR/counter_video_180.mp4:id=vp1:Period=P1 \
$TEMP_DIR/counter_audio.mp4:id=ap2:Period=P2 $TEMP_DIR/counter_video_180.mp4:id=vp2:Period=P2" "dash-multiperiod"

do_hash_test $TEMP_DIR/file.mpd "mpd"
do_hash_test $TEMP_DIR/vp1-.mp4 "initp1"
do_hash_test $TEMP_DIR/vp2-.mp4 "initp2"

#correct but currently broken in gpac
#do_playback_test "$TEMP_DIR/file.mpd" "play-dash-live-periods"

test_end

