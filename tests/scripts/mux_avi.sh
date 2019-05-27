
test_avimx()
{

test_begin "mux-avi-$1"
if [ $test_skip  = 1 ] ; then
return
fi

dstfile="$TEMP_DIR/test.avi"

do_test "$GPAC -i $MEDIA_DIR/auxiliary_files/count_video.cmp -i $MEDIA_DIR/auxiliary_files/count_english.mp3 $2 -o $dstfile" "mux"
if [ $3 != 0 ] ; then
do_hash_test $dstfile "mux"
fi

myinspect=$TEMP_DIR/inspect.txt
do_test "$GPAC -i $dstfile inspect:all:fmt=%cts%-%size%%lf%:interleave=false:log=$myinspect -graph -stats"
do_hash_test $myinspect "inspect"

test_end

}

test_avimx "compressed" "" 1
#don't hash uncompressed file
test_avimx "uncompressed" "-blacklist=vtbdec,nvdec reframer:raw @" 0
