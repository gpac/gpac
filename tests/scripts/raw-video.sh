#test raw video

DST="127.0.0.1"
IFCE="127.0.0.1"

raw_test ()
{

test_begin "raw-vid-$1"
if [ $test_skip  = 1 ] ; then
return
fi

rawfile=$TEMP_DIR/raw.$1
do_test "$GPAC -i $mp4file -o $rawfile -blacklist=vtbdec,nvdec,ohevcdec" "dump"
do_hash_test "$rawfile" "dump"


myinspect="inspect:fmt=@pn@-@cts@-@bo@@lf@"

insfile=$TEMP_DIR/dump.txt
do_test "$GPAC -i $rawfile:size=128x128 $myinspect:log=$insfile" "inspect"
do_hash_test "$insfile" "inspect"

#only do the reverse playback test for yuv (same for the other formats)
if [ $1  = "yuv" ] ; then

insfile=$TEMP_DIR/dumpns.txt
do_test "$GPAC -i $rawfile:size=128x128 $myinspect:speed=-1:log=$insfile" "inspect_reverse"
do_hash_test "$insfile" "inspect_reverse"

fi

#check GPU output on frame 20 (first few frames are fade to black ...
#test is commented for now (hashes are ok), due to buildbot stability with GPU access
#gpudump=$TEMP_DIR/gpudump.rgb
#do_test "$GPAC -i $rawfile:size=128x128 vout:dumpframes=20:out=$gpudump" "gpu_dump"
#do_hash_test "$gpudump" "gpu_dump"

#test video cropping filter in forward mode
cropfile=$TEMP_DIR/dumpcrop.$1
do_test "$GPAC -i $rawfile:size=128x128 vcrop:wnd=32x10x64x64 @ -o $cropfile" "crop"
do_hash_test "$cropfile" "crop"

#test video cropping filter in copy mode
cropfile=$TEMP_DIR/dumpcropcp.$1
do_test "$GPAC -i $rawfile:size=128x128 vcrop:copy:wnd=32x10x64x64 @ -o $cropfile" "crop"
#use same hash as before, they shall be identical
do_hash_test "$cropfile" "crop"

test_end
}

mp4file="$TEMP_DIR/vid.mp4"
$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264:dur=1 -new $mp4file 2> /dev/null

#complete lists of pixel formats extensions in gpac - we don't test all of these
#pfstr="yuv yuvl yuv2 yp2l yuv4 yp4l uyvy vyuy yuyv yvyu nv12 nv21 nv1l nv2l yuva yuvd grey gral rgb4 rgb5 rgb6 rgba argb rgb bgr xrgb rgbx xbgr bgrx rgbd rgbds rgbs rgbas"
#the ones we test for now - nv1l is commented (no support in old ffmpeg used on gpac buildbot) and alpha not yet tested
pfstr="yuv yuvl yuv2 yp2l yuv4 yp4l uyvy vyuy yuyv yvyu nv12 nv21 grey rgb bgr xrgb rgbx xbgr bgrx"

for i in $pfstr ; do
	raw_test $i
done

