#!/bin/sh

test_begin "dash"

do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264 -add $MEDIA_DIR/auxiliary_files/enst_audio.aac -new $TEMP_DIR/file.mp4" "dash-input-preparation"

do_test "$MP42TS -src $TEMP_DIR/file.mp4 -dst-file=$TEMP_DIR/file.ts" "ts-for-dash-input-preparation"

do_test "$MP4BOX -dash 1000 $TEMP_DIR/file.mp4 -out $TEMP_DIR/file.mpd" "basic-dash"

do_test "$MP4BOX -dash 1000 $TEMP_DIR/file.ts -out $TEMP_DIR/file2.mpd" "ts-dash"

do_test "$MP4BOX -bs-switching single -mpd-refresh 1000 -dash-ctx $TEMP_DIR/dash_ctx -dash-run-for 20000 -subdur 100 -dynamic -profile live -dash-live 100 $TEMP_DIR/file.mp4 -out $TEMP_DIR/file3.mpd" "dash-live"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter-10mn_I25_baseline_320x180_128kbps.264 -new $TEMP_DIR/counter_video_180.mp4" "multiperiod-input-preparation_180"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter-10mn_I25_baseline_640x360_192kbps.264 -new $TEMP_DIR/counter_video_360.mp4" "multiperiod-input-preparation2_320"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter-10mn_I25_baseline_1280x720_512kbps.264 -new $TEMP_DIR/counter_video_720.mp4" "multiperiod-input-preparation3_720"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter-10mn_I25_baseline_1920x1080_768kbps.264 -new $TEMP_DIR/counter_video_1080.mp4" "multiperiod-input-preparation4_1080"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter-10mn_audio.aac -new $TEMP_DIR/counter_audio.mp4" "multiperiod-input-preparation_audio"

do_test "$MP4BOX -bs-switching no -dash 10000 -frag 1000 -dash-profile live \
-out $TEMP_DIR/file4.mpd -segment-name mp4-live-periods-\$RepresentationID\$-\$Number\$ \
$TEMP_DIR/counter_audio.mp4:id=aaclc:Period=P1 $TEMP_DIR/counter_video_1080.mp4:id=h264bl_full:Period=P1 \
$TEMP_DIR/counter_video_180.mp4:id=h264bl_low:Period=P1 \
$TEMP_DIR/counter_audio.mp4:id=aaclc_p2:Period=P2 $TEMP_DIR/counter_video_360.mp4:id=h264bl_mid:Period=P2 \
$TEMP_DIR/counter_video_720.mp4:id=h264bl_hd:Period=P2" "dash-multiperiod"

do_playback_test "$TEMP_DIR/file.mpd" "play-basic-dash"

do_playback_test "$TEMP_DIR/file2.mpd" "play-dash-ts"

do_playback_test "$TEMP_DIR/file3.mpd" "play-dash-live"

do_playback_test "$TEMP_DIR/file4.mpd" "play-dash-live-periods"

test_end
