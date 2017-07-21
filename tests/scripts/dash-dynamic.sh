#!/bin/sh

test_begin "dash-dynamic-simple"

$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264:dur=10 -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.aac:dur=10 -new $TEMP_DIR/file.mp4

do_test "$MP4BOX -dash-live 2000 -dynamic -profile live -bs-switching single -mpd-refresh 10 -dash-ctx $TEMP_DIR/dash_ctx -time-shift -1 -dash-run-for 6000 $TEMP_DIR/file.mp4#video $TEMP_DIR/file.mp4#audio -out $TEMP_DIR/file.mpd" "dash-gen" &

sleep 1

do_playback_test "$TEMP_DIR/file.mpd" "dash-play"

#wait for the end of the MP4Box
wait

#this is dynamic, use the stored MPD to avoid the varying availablilityStartTime
do_hash_test $TEMP_DIR/file-Period_DID1.mpd "hash-mpd"

do_hash_test $TEMP_DIR/file_dash_track1_3.m4s "hash-seg3-video"

do_hash_test $TEMP_DIR/file_dash_track2_3.m4s "hash-seg3-audio"

test_end

