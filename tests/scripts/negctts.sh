#!/bin/sh
test_begin "import-File-with-negative-CTS-DTS-offsets"


do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/3D-HEVC/stream_bbb.bit:fmt=HEVC:negctts $TEMP_DIR/test.mp4" "mp4box-import-negctts"

do_hash_test "$TEMP_DIR/test.mp4" "import"

do_playback_test "$TEMP_DIR/test.mp4" "play-mp4"

do_test "$MP42TS -src $TEMP_DIR/test.mp4 -dst-file $TEMP_DIR/test.ts" "mp42ts"


do_playback_test "$TEMP_DIR/test.ts" "play-ts"

test_end
