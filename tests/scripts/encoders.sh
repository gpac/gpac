#!/bin/sh

test_encoder()
{

test_begin "encoder-$1"

if [ $test_skip  = 1 ] ; then
return
fi

dst_file=$TEMP_DIR/$3

do_test "$GPAC -i $2 $4 -o $dst_file -graph -stats $5"  "encoder"

#some decoder output tests are disabled due to non uniform output on different platform
#this will need further investigations
if [ $6  = 0 ] ; then
do_hash_test "$dst_file" "encoder"
fi

if [ ! -f $dst_file ] ; then
result="Encoded output not present"
#else
#gpac play $dst_file
fi

mp4_file=$TEMP_DIR/in.mp4
#also do the test from MP4
$MP4BOX -add $2:dur=0.4 -new $mp4_file 2> /dev/null

dst_file=$TEMP_DIR/mp4-$3
do_test "$GPAC -i $mp4_file $4 -o $dst_file -graph -stats $5"  "encoder-mp4"
if [ $6  = 0 ] ; then
do_hash_test "$dst_file" "encoder-mp4"
fi

if [ ! -f $dst_file ] ; then
result="Encoded output from MP4 not present"
#else
#gpac play $dst_file
fi

test_end

}

jpeg=`$GPAC -h filters 2>&1 | grep jpgenc`
png=`$GPAC -h filters 2>&1 | grep pngenc`

x264ff=`gpac -hh ffenc:* 2>&1 | grep ffenc:libx264`
j2kff=`gpac -hh ffenc:* 2>&1 | grep ffenc:jpeg2000`

#video encoder tests

#test png encode
if [ -n "$png" ] ; then
test_encoder "png-pngenc" $MEDIA_DIR/auxiliary_files/sky.jpg "test.png" "" "-blacklist=ffenc" 0
fi
test_encoder "png-ffenc" $MEDIA_DIR/auxiliary_files/sky.jpg "test.png" "" "-blacklist=pngenc" 0

#test jpeg encode
if [ -n "$jpeg" ] ; then
test_encoder "jpeg-jpgenc" $MEDIA_DIR/auxiliary_files/logo.png "test.jpg" "" "-blacklist=ffenc" 0
fi
test_encoder "jpeg-ffenc" $MEDIA_DIR/auxiliary_files/logo.png "test.jpg" "" "-blacklist=jpgenc" 0

#test mpeg4 part2 encode
test_encoder "m4v-ffenc" $MEDIA_DIR/auxiliary_files/enst_video.h264 "test.cmp" "" "-blacklist=vtbdec,nvdec,ohevcdec" 0

#test h263 encode - insert a crop filter to produce a valid h263 frame size
test_encoder "h263-ffenc" $MEDIA_DIR/auxiliary_files/enst_video.h264 "test.263" "vcrop:wnd=0x0x128x96 @" "-blacklist=vtbdec,nvdec,ohevcdec" 0

#test m1v encode
test_encoder "mpeg1-ffenc" $MEDIA_DIR/auxiliary_files/enst_video.h264 "test.m1v" "" "-blacklist=vtbdec,nvdec,ohevcdec" 0

#test m2v encode
test_encoder "mpeg2-ffenc" $MEDIA_DIR/auxiliary_files/enst_video.h264 "test.m2v" "" "-blacklist=vtbdec,nvdec,ohevcdec" 0


#test AVC encode
if [ -n "$x264ff" ] ; then
test_encoder "avc-ffenc" $MEDIA_DIR/auxiliary_files/count_video.cmp "test.264" "" "-blacklist=vtbdec,nvdec,ohevcdec" 0
fi

#test MJ2 encode - we need to explicetly add the encoder, since the isom muxer can accept any input
if [ -n "$j2kff" ] ; then
test_encoder "j2k-ffenc" $MEDIA_DIR/auxiliary_files/count_video.cmp "test.mj2" "enc:c=j2k @" "-blacklist=vtbdec,nvdec,ohevcdec" 0
fi



#audio encoder tests
test_encoder "mp3-ffenc" $MEDIA_DIR/auxiliary_files/enst_audio.aac "test.mp3" "" "" 0

test_encoder "aac-ffenc" $MEDIA_DIR/auxiliary_files/count_french.mp3 "test.aac" "" "" 0

test_encoder "ac3-ffenc" $MEDIA_DIR/auxiliary_files/count_french.mp3 "test.ac3" "" "" 0

