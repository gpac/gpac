#!/bin/sh

temp_file=$TEMP_DIR/test.mp4

rext_test() {

test_begin $1
 if [ $test_skip  = 1 ] ; then
  return
 fi

do_test "$MP4BOX -add $2 -new $temp_file" "import"
do_hash_test "$temp_file" "import"

do_playback_test "$temp_file" "play"

test_end
}

#avc range extension
rext_test "avc-420-8bit" "$EXTERNAL_MEDIA_DIR/rext/counter-bifs-10min-320x180-bipbop_420.h264"
rext_test "avc-422-8bit" "$EXTERNAL_MEDIA_DIR/rext/counter-bifs-10min-320x180-bipbop_422.h264"
rext_test "avc-444-8bit" "$EXTERNAL_MEDIA_DIR/rext/counter-bifs-10min-320x180-bipbop_444.h264"
rext_test "avc-420-10bit" "$EXTERNAL_MEDIA_DIR/rext/counter-bifs-10min-320x180-bipbop_420_10.h264"
rext_test "avc-422-10bit" "$EXTERNAL_MEDIA_DIR/rext/counter-bifs-10min-320x180-bipbop_422_10.h264"
rext_test "avc-444-10bit" "$EXTERNAL_MEDIA_DIR/rext/counter-bifs-10min-320x180-bipbop_444_10.h264"

#hevc range extension
rext_test "hevc-420-8bit" "$EXTERNAL_MEDIA_DIR/rext/counter-bifs-1min-640x360-bipbop_420.hvc"
rext_test "hevc-422-8bit" "$EXTERNAL_MEDIA_DIR/rext/counter-bifs-1min-640x360-bipbop_422.hvc"
rext_test "hevc-444-8bit" "$EXTERNAL_MEDIA_DIR/rext/counter-bifs-1min-640x360-bipbop_444.hvc"
rext_test "hevc-420-10bit" "$EXTERNAL_MEDIA_DIR/rext/counter-bifs-1min-640x360-bipbop_420_10.hvc"
rext_test "hevc-422-10bit" "$EXTERNAL_MEDIA_DIR/rext/counter-bifs-1min-640x360-bipbop_422_10.hvc"
rext_test "hevc-444-10bit" "$EXTERNAL_MEDIA_DIR/rext/counter-bifs-1min-640x360-bipbop_444_10.hvc"

