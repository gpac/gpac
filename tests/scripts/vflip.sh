#test vflip filter

vflip_test ()
{

test_begin "vflip-$1"
if [ $test_skip  = 1 ] ; then
return
fi

rawfile=$TEMP_DIR/raw.$1
do_test "$GPAC -i $mp4file -o $rawfile -blacklist=vtbdec,nvdec,ohevcdec" "dump"

#test vertical flip
flipfile=$TEMP_DIR/dumpflipv.$1
do_test "$GPAC -i $rawfile:size=128x128 vflip @ -o $flipfile" "flipv"
do_hash_test_bin "$flipfile" "flipv"

do_play_test "play-v" "$flipfile:size=128x128" ""


#test horizontal flip
flipfile=$TEMP_DIR/dumpfliph.$1
do_test "$GPAC -i $rawfile:size=128x128 vflip:horiz @ -o $flipfile" "fliph"
do_hash_test_bin "$flipfile" "fliph"

do_play_test "play-h" "$flipfile:size=128x128" ""


if [ "$1" = "yuv" ] ; then

#test both directions flip
flipfile=$TEMP_DIR/dumpfliphv.$1
do_test "$GPAC -i $rawfile:size=128x128 vflip:both @ -o $flipfile" "fliphv"
do_hash_test_bin "$flipfile" "fliphv"

#gpac -i $flipfile:size=128x128 vout

#test no flip
flipfile=$TEMP_DIR/dumpflipno.$1
do_test "$GPAC -i $rawfile:size=128x128 vflip:off @ -o $flipfile -graph" "flipoff"
do_hash_test_bin "$flipfile" "flipoff"

#gpac -i $flipfile:size=128x128 vout

nvdec=`$GPAC -h filters 2>&1 | grep nvdec`

if [ -n "$nvdec" ] ; then
#test Frame interface with nvdec - todo, check with vtbdec. It may be triggered by nvdia decoder
do_test "$GPAC -blacklist=ffdec,ohevcdec -i $MEDIA_DIR/auxiliary_files/enst_video.h264:dur=1 vflip:horiz:fmode=single @ -o $flipfile -graph" "fliph_dec"
do_hash_test_bin "$flipfile" "fliph_dec"
fi

#gpac -i $flipfile:size=128x128 vout

fi


test_end
}

mp4file="$TEMP_DIR/vid.mp4"
$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264:dur=1 -new $mp4file 2> /dev/null

#complete lists of pixel formats extensions in gpac - we don't test all of these
#pfstr="yuv yuvl yuv2 yp2l yuv4 yp4l uyvy vyuy yuyv yvyu nv12 nv21 nv1l nv2l yuva yuvd grey gral rgb4 rgb5 rgb6 rgba argb rgb bgr xrgb rgbx xbgr bgrx rgbd rgbds rgbs rgbas"
#the ones we test for now - nv1l is commented (no support in old ffmpeg used on gpac buildbot) and alpha not yet tested
pfstr="yuv yuvl yuv2 yp2l yuv4 yp4l uyvy vyuy yuyv yvyu nv12 nv21 grey rgb bgr xrgb rgbx xbgr bgrx"

#pfstr="rgb"

for i in $pfstr ; do
	vflip_test $i
done

