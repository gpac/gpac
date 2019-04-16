
test_avimx()
{

test_begin "mux-avi-$1"
if [ $test_skip  = 1 ] ; then
return
fi

dstfile="$TEMP_DIR/test.avi"

do_test "$GPAC -i $MEDIA_DIR/auxiliary_files/count_video.cmp -i $MEDIA_DIR/auxiliary_files/count_english.mp3 $2 -o $dstfile" "mux"
do_hash_test $dstfile "mux"

test_end

}

test_avimx "compressed" ""
test_avimx "uncompressed" "-blacklist=vtbdec,nvdec reframer:raw @"
