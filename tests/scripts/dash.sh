#!/bin/sh

test_begin "dash"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264 -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.aac -new $TEMP_DIR/file.mp4" "dash-input-preparation"

do_test "$MP4BOX -dash 1000 $TEMP_DIR/file.mp4#video $TEMP_DIR/file.mp4#audio -out $TEMP_DIR/file.mpd" "basic-dash"

do_hash_test $TEMP_DIR/file.mpd "basic-dash-hash-mpd"

do_hash_test $TEMP_DIR/file_track1_dashinit.mp4 "basic-dash-hash-init-segment-video"

do_hash_test $TEMP_DIR/file_track2_dashinit.mp4 "basic-dash-hash-init-segment-audio"

do_playback_test "$TEMP_DIR/file.mpd" "play-basic-dash"

test_end
