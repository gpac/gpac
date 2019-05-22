ts_test ()
{

test_begin "tsmux-$1"
if [ $test_skip  = 1 ] ; then
return
fi

tsfile="$TEMP_DIR/test.ts"

do_test "$GPAC $2 -o $tsfile:pcr_init=0:pes_pack=none$3" "mux"
do_hash_test $tsfile "mux"

myinspect=$TEMP_DIR/inspect.txt
do_test "$GPAC -i $tsfile inspect:all:deep:interleave=false:log=$myinspect" "inspect"
do_hash_test $myinspect "inspect"

myinspect=$TEMP_DIR/inspect-frame.txt
do_test "$GPAC -i $tsfile inspect:mode=frame:all:deep:interleave=false:log=$myinspect" "inspect-frame"
do_hash_test $myinspect "inspect-frame"

test_end
}

mp4file="$TEMP_DIR/test.mp4"

$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264 -add $MEDIA_DIR/auxiliary_files/enst_audio.aac -new $mp4file 2> /dev/null


ts_test "simple" "-i $mp4file" ""

ts_test "rate" "-i $mp4file" ":rate=1m"

ts_test "temi" "-i $mp4file" ":temi=4,http://localhost/"

ts_test "pcr" "-i $mp4file" ":max_pcr=40:pcr_only:pcr_offset=30000:flush_rap"

ts_test "sdt" "-i $mp4file" ":name=GPACTest:provider:GPAC:sdt_rate=500"

ts_test "srt" "-i $MEDIA_DIR/auxiliary_files/subtitle.srt" ""

ts_test "webvtt" "-i $MEDIA_DIR/auxiliary_files/subtitle.srt:webvtt" ""

ts_test "ac3" "-i $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.ac3" ""

rm $mp4file
