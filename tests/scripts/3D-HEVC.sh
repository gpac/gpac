#!/bin/sh
test_begin "import-3D-HEVC"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/3D-HEVC/stream_bbb.bit:fmt=HEVC $EXTERNAL_MEDIA_DIR/stream_bbb.mp4" "mp4box-import-3D-HEVC"
do_hash_test $TEMP_DIR/test.mp4 "mp4box-import-3D-HEVC"

do_playback_test "$EXTERNAL_MEDIA_DIR/stream_bbb.mp4" "mp4client-play"

test_end

test_begin "add-subsamples-HEVC"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/3D-HEVC/stream_bbb.bit:fmt=HEVC:subsamples $TEMP_DIR/stream_bbb_subsamples.mp4" "mp4box-subsamples-HEVC"
do_hash_test $TEMP_DIR/test.mp4 "mp4box-subsamples-HEVC"

test_end4
