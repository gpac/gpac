#!/bin/sh

test_evg_pixfmt()
{

test_begin "evg-pixfmt-$1"

if [ $test_skip  = 1 ] ; then
return
fi

src_file=$EXTERNAL_MEDIA_DIR/raw/raw.rgb
bt_file=$EXTERNAL_MEDIA_DIR/raw/overlay.bt
dst_file=$TEMP_DIR/dump.$1
dst_file2=$TEMP_DIR/dump.rgb
dst_file3=$TEMP_DIR/compose.$1

#test rgb -> format
do_test "$GPAC -i $src_file:size=128x128 compositor:!softblt:opfmt=$1 @ -o $dst_file  -graph -stats"  "rgb_$1"
do_hash_test "$dst_file" "rgb_$1"

do_play_test "play" "$dst_file:size=128x128"

#test format -> rgb
do_test "$GPAC -i $dst_file:size=128x128 compositor:!softblt:opfmt=rgb @ -o $dst_file2  -graph -stats"  "$1_rgb"
do_hash_test "$dst_file2" "$1_rgb"

do_play_test "play" "$dst_file2:size=128x128"

#test 2D rasterizer in format destination (passthrough with direct overlay on raw data)
#note that we force using a GNU Free Font SANS to make sure we always use the same font on all platforms
do_test "$GPAC -font-dirs=$EXTERNAL_MEDIA_DIR/fonts/ -rescan-fonts -i $dst_file:size=128x128 -i $bt_file compositor:!softblt:drv=no @ -o $dst_file3  -graph -stats"  "compose_$1"
do_hash_test "$dst_file3" "compose_$1"

do_play_test "play" "$dst_file3:size=128x128"

test_end

}


#complete lists of pixel formats extensions in gpac - we don't test all of these
#pfstr="yuv yuvl yuv2 yp2l yuv4 yp4l uyvy vyuy yuyv yvyu nv12 nv21 nv1l nv2l yuva yuvd grey algr gral rgb4 rgb5 rgb6 rgba argb bgra abgr rgb bgr xrgb rgbx xbgr bgrx rgbd rgbds rgbs rgbas"
#the ones we test for now - only grey, greyscale, 16/24/32 bits RGB, all YUV. We exclude rgb from the list since this is the format of our source test and destination test
pfstr="yuv yuvl yuv2 yp2l yuv4 yp4l uyvy vyuy yuyv yvyu nv12 nv21 nv1l nv2l grey algr gral rgb4 rgb5 rgb6 rgba argb bgra abgr bgr xrgb rgbx xbgr bgrx"

for i in $pfstr ; do
	test_evg_pixfmt $i
done
