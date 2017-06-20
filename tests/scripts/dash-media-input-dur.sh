#!/bin/sh

TEST_NAME="dash-input-dur"

test_begin "$TEST_NAME"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264 -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.aac -new $TEMP_DIR/file.mp4" "dash-input-preparation"

do_test "$MP4BOX -dash 1000 $TEMP_DIR/file.mp4#video:dur=17 $TEMP_DIR/file.mp4#audio:dur=17 -out $TEMP_DIR/file.mpd" "$TEST_NAME-dash"

do_hash_test $TEMP_DIR/file.mpd "$TEST_NAME-hash-mpd"

do_hash_test $TEMP_DIR/file_track1_dashinit.mp4 "$TEST_NAME-hash-init-segment-video"

do_hash_test $TEMP_DIR/file_track2_dashinit.mp4 "$TEST_NAME-hash-init-segment-audio"

do_playback_test "$TEMP_DIR/file.mpd" "play-$TEST_NAME"

test_end
