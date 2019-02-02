#!/bin/sh

#rewind video while dumping to yuv

mp4file="$TEMP_DIR/file.mp4"


test_video()
{
#rewind needs negative speed playback support, build an MP4
$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264:dur=1 -new $mp4file 2> /dev/null

test_begin "rewind-video"

if [ $test_skip  = 1 ] ; then
return
fi

dumpfile=$TEMP_DIR/dump.yuv
do_test "$GPAC -i $mp4file rewind @ -o $dumpfile:speed=-1 -blacklist=vtbdec,ohevcdec,nvdec"  "rewind"
do_hash_test "$dumpfile" "rewind"

test_end

}

#rewind audio while dumping to pcm

test_audio()
{
#rewind needs negative speed playback support, build an MP4
$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_audio.aac:dur=0.4 -new $mp4file 2> /dev/null

do_pcm_hashes=1
#on linux 32 bit we for now disable the hashes, they all differ due to different float/double precision
config=`gpac -h bin 2>&1 | grep GPAC_HAS_64`

if [ -z $config ] ; then
config=`gpac -h bin 2>&1 | grep GPAC_CONFIG_LINUX`

if [ -n "$config" ] ; then
do_pcm_hashes=0
fi
fi

test_begin "rewind-audio"

if [ $test_skip  = 1 ] ; then
return
fi

dumpfile=$TEMP_DIR/dump.pcm
do_test "$GPAC -i $mp4file rewind @ -o $dumpfile:speed=-1"  "rewind"
if [ $do_pcm_hashes != 0 ] ; then
do_hash_test "$dumpfile" "rewind"
fi

test_end
}

test_video

test_audio


