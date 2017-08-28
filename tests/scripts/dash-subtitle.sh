#!/bin/sh

test_begin "dash-subt"

do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/subtitle.srt -new $TEMP_DIR/file.mp4" "dash-input-preparation"

do_test "$MP4BOX -dash 1500 $TEMP_DIR/file.mp4 -profile live -out $TEMP_DIR/file.mpd" "dash-subt"

do_hash_test $TEMP_DIR/file.mpd "dash-subt-hash-mpd"

do_playback_test "$TEMP_DIR/file.mpd" "play-dash-subt"

test_end
