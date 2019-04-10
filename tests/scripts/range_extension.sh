#!/bin/sh


rext_test() {

test_begin $1
 if [ $test_skip  = 1 ] ; then
  return
 fi

temp_file=$TEMP_DIR/test.mp4
temp_dump=$TEMP_DIR/dump.$3

do_test "$MP4BOX -add $2 -new $temp_file" "import"
do_hash_test "$temp_file" "import"

#decode and dump 2 frames
do_test "$GPAC -blacklist=vtbdec,ohevcdec,nvdec -i $temp_file -o $temp_dump:sstart=8:send=9" "decode"
do_hash_test "$temp_dump" "decode"

case $2 in
*320x180*)
 do_play_test "play" "$temp_dump:size=320x180";;
*640x360*)
 do_play_test "play" "$temp_dump:size=640x360";;
esac

test_end
}

#avc range extension
rext_test "avc-420-8bit" "$EXTERNAL_MEDIA_DIR/rext/counter-bifs-10min-320x180-bipbop_420.h264" "yuv"
rext_test "avc-422-8bit" "$EXTERNAL_MEDIA_DIR/rext/counter-bifs-10min-320x180-bipbop_422.h264" "yuv2"
rext_test "avc-444-8bit" "$EXTERNAL_MEDIA_DIR/rext/counter-bifs-10min-320x180-bipbop_444.h264" "yuv4"
rext_test "avc-420-10bit" "$EXTERNAL_MEDIA_DIR/rext/counter-bifs-10min-320x180-bipbop_420_10.h264" "yuvl"
rext_test "avc-422-10bit" "$EXTERNAL_MEDIA_DIR/rext/counter-bifs-10min-320x180-bipbop_422_10.h264" "yp2l"
rext_test "avc-444-10bit" "$EXTERNAL_MEDIA_DIR/rext/counter-bifs-10min-320x180-bipbop_444_10.h264" "yp4l"

#hevc range extension
rext_test "hevc-420-8bit" "$EXTERNAL_MEDIA_DIR/rext/counter-bifs-1min-640x360-bipbop_420.hvc" "yuv"
rext_test "hevc-422-8bit" "$EXTERNAL_MEDIA_DIR/rext/counter-bifs-1min-640x360-bipbop_422.hvc" "yuv2"
rext_test "hevc-444-8bit" "$EXTERNAL_MEDIA_DIR/rext/counter-bifs-1min-640x360-bipbop_444.hvc" "yuv4"
rext_test "hevc-420-10bit" "$EXTERNAL_MEDIA_DIR/rext/counter-bifs-1min-640x360-bipbop_420_10.hvc" "yuvl"
rext_test "hevc-422-10bit" "$EXTERNAL_MEDIA_DIR/rext/counter-bifs-1min-640x360-bipbop_422_10.hvc" "yp2l"
rext_test "hevc-444-10bit" "$EXTERNAL_MEDIA_DIR/rext/counter-bifs-1min-640x360-bipbop_444_10.hvc" "yp4l"

