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
flipfile=$TEMP_DIR/dumpflip.$1
do_test "$GPAC -i $rawfile:size=128x128 vflip @ -o $flipfile" "flipv"
do_hash_test "$flipfile" "flipv"

gpac -i $flipfile:size=128x128 vout 


#test horizontal flip
do_test "$GPAC -i $rawfile:size=128x128 vflip:horiz @ -o $flipfile" "fliph"
do_hash_test "$flipfile" "fliph"

gpac -i $flipfile:size=128x128 vout


#test both directions flip
do_test "$GPAC -i $rawfile:size=128x128 vflip:both @ -o $flipfile" "fliphv"
do_hash_test "$flipfile" "fliphv"

gpac -i $flipfile:size=128x128 vout

#test no flip
do_test "$GPAC -i $rawfile:size=128x128 vflip:off @ -o $flipfile -graph" "flipoff"
do_hash_test "$flipfile" "flipoff"

gpac -i $flipfile:size=128x128 vout


#test Frame interface. It may be triggered by nvdia decoder
do_test "$GPAC -blacklist=ffdec,ohevcdec -i $MEDIA_DIR/auxiliary_files/enst_video.h264:dur=1 vflip:horiz:fmode=single @ -o $flipfile -graph" "fliph_dec"
do_hash_test "$flipfile" "fliph_dec"

gpac -i $flipfile:size=128x128 vout


test_end
}

mp4file="$TEMP_DIR/vid.mp4"
$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264:dur=1 -new $mp4file 2> /dev/null

#complete lists of pixel formats extensions in gpac - we don't test all of these
#pfstr="yuv yuvl yuv2 yp2l yuv4 yp4l uyvy vyuy yuyv yvyu nv12 nv21 nv1l nv2l yuva yuvd grey gral rgb4 rgb5 rgb6 rgba argb rgb bgr xrgb rgbx xbgr bgrx rgbd rgbds rgbs rgbas"
#the ones we test for now - nv1l is commented (no support in old ffmpeg used on gpac buildbot) and alpha not yet tested
pfstr="yuv yuvl yuv2 yp2l yuv4 yp4l uyvy vyuy yuyv yvyu nv12 nv21 grey rgb bgr xrgb rgbx xbgr bgrx"

#not working yuvl yp2l yp4l
#pfstr="nv12 nv21"

for i in $pfstr ; do
	vflip_test $i
done

