#!/bin/sh
test_begin "import-File-with-negative-CTS-DTS-offsets"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/3D-HEVC/stream_bbb.bit:fmt=HEVC:negctts $TEMP_DIR/stream_bbb_negctts.mp4" "mp4box-import-negctts"
do_hash_test $TEMP_DIR/test.mp4 "mp4box-import-negctts"
test_end
