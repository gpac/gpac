#!/bin/sh

test_begin "dash-ts"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264 -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.aac -new $TEMP_DIR/file.mp4" "ts-for-dash-input-preparation"

do_test "$MP42TS -src $TEMP_DIR/file.mp4 -dst-file=$TEMP_DIR/file.ts" "ts-for-dash-input-preparation-2"

do_test "$MP4BOX -dash 1000 -rap -single-file -segment-name myrep/ts-segment-single-f-\$RepresentationID\$ $TEMP_DIR/file.ts -out $TEMP_DIR/file1.mpd" "ts-dash-single-file"

do_playback_test "$TEMP_DIR/file1.mpd" "play-ts-dash-single-file"

do_test "$MP4BOX -dash 1000 -rap -segment-name myrep/ts-segment-multiple-f-\$RepresentationID\$ $TEMP_DIR/file.ts -out $TEMP_DIR/file2.mpd" "ts-dash-multiple-file"

do_playback_test "$TEMP_DIR/file2.mpd" "play-ts-dash-multisegment"

test_end
