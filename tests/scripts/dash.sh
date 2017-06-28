#!/bin/sh

test_begin "dash-basic"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264 -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.aac -new $TEMP_DIR/file.mp4" "input-preparation"
do_hash_test "$TEMP_DIR/file.mp4" "input-preparation"


if [ -f "$TEMP_DIR/file.mpd" ] ; then
	rm "$TEMP_DIR/file.mpd"
fi

do_test "$MP4BOX -dash 1000 $TEMP_DIR/file.mp4#video $TEMP_DIR/file.mp4#audio -out $TEMP_DIR/file.mpd" "mpd"

do_hash_test "$TEMP_DIR/file.mpd" "mpd"
do_hash_test "$TEMP_DIR/file_track1_dashinit.mp4" "segment-video"
do_hash_test "$TEMP_DIR/file_track2_dashinit.mp4" "segment-audio"

do_playback_test "$TEMP_DIR/file.mpd" "play"

test_end
