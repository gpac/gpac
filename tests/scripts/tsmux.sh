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
do_test "$GPAC -i $tsfile inspect:all:deep:interleave=false:log=$myinspect"
do_hash_test $myinspect "inspect"

test_end
}

mp4file="$TEMP_DIR/test.mp4"

$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264 -add $MEDIA_DIR/auxiliary_files/enst_audio.aac -new $mp4file 2> /dev/null


ts_test "simple" "-i $mp4file" ""

ts_test "rate" "-i $mp4file" ":rate=1m"

ts_test "temi" "-i $mp4file" ":temi=4,http://localhost/"

ts_test "pcr" "-i $mp4file" ":max_pcr=40:pcr_only:pcr_offset=30000:flush_rap"

rm $mp4file
