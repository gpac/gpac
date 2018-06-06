#!/bin/sh

test_begin "dash-cenc"

do_test "$MP4BOX -crypt $MEDIA_DIR/encryption/cbc.xml -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264 -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.aac -new $TEMP_DIR/file.mp4" "dash-input-playready-encrypt"

do_test "$MP4BOX -dash 1000 -profile dashavc264:live $TEMP_DIR/file.mp4#video $TEMP_DIR/file.mp4#audio -out $TEMP_DIR/file.mpd" "dashing-cenc-playready"

do_playback_test "$TEMP_DIR/file.mpd" "play-dash-playready-cenc"

test_end

