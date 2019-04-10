#!/bin/sh
 if [ $EXTERNAL_MEDIA_AVAILABLE = 0 ] ; then
  return
 fi


test_begin "3D-HEVC"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/3D-HEVC/stream_bbb.bit:fmt=HEVC -new $TEMP_DIR/test.mp4" "import"

ohevcdec=`$GPAC -h filters 2>&1 | grep ohevc`

#test openhevc decoding of stereo
if [ -n "$ohevcdec" ] ; then
 do_test "$GPAC -blacklist=nvdec,vtbdec,ffdec -i $TEMP_DIR/test.mp4 -o $TEMP_DIR/dump.yuv:force_stereo:sstart=7:send=7" "decode"
 do_hash_test_bin $TEMP_DIR/dump.yuv "decode"

 do_play_test "play" "$TEMP_DIR/dump.yuv:size=1920x2160"
fi

test_end



test_begin "add-subsamples-HEVC"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/3D-HEVC/stream_bbb.bit:fmt=HEVC:subsamples -new $TEMP_DIR/test.mp4" "add-subsamples-HEVC"
do_hash_test $TEMP_DIR/test.mp4 "import"

test_end
