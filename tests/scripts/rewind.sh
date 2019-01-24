#rewind video while dumping to yuv

#rewind needs negative speed playback, build an MP4
mp4file="$TEMP_DIR/file.mp4"


$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264:dur=1 -new $mp4file 2> /dev/null

test_begin "rewind-video"

if [ $test_skip  = 1 ] ; then
return
fi

dumpfile=$TEMP_DIR/dump.yuv
do_test "$GPAC -i $mp4file rewind @ -o $dumpfile:speed=-1 -blacklist=vtbdec,ohevcdec,nvdec"  "rewind"
do_hash_test "$dumpfile" "rewind"

#uncomment  to playback result
#$GPAC -i $dumpfile:size=128x128 vout

test_end


#rewind audio while dumping to pcm

#rewind needs negative speed playback, build an MP4
$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_audio.aac:dur=1 -new $mp4file 2> /dev/null

test_begin "rewind-audio"

if [ $test_skip  = 1 ] ; then
return
fi

dumpfile=$TEMP_DIR/dump.pcm
do_test "$GPAC -i $mp4file rewind @ -o $dumpfile:speed=-1"  "rewind"
do_hash_test "$dumpfile" "rewind"

#uncomment  to playback result
#$GPAC -i $dumpfile:sr=48000:ch=2 aout

test_end

