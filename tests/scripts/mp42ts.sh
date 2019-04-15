#test MP4Box RTP streaming and MP4 client

#DST="-dst=239.255.0.1"
DST=""

IFCE="127.0.0.1"


ts_test ()
{

test_begin "mp42ts-$1"
if [ $test_skip  = 1 ] ; then
return
fi

tsfile="$TEMP_DIR/test.ts"

do_test "$MP42TS $2 -dst-file=$tsfile" "mux"
do_hash_test $tsfile "mux"

 myinspect=$TEMP_DIR/inspect.txt
 do_test "$GPAC -i $tsfile inspect:all:deep:interleave=false:log=$myinspect"
 do_hash_test $myinspect "inspect"

test_end
}

mp4file="$TEMP_DIR/test.mp4"

$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264 -add $MEDIA_DIR/auxiliary_files/enst_audio.aac -new $mp4file 2> /dev/null


ts_test "simple" "-src $mp4file -pcr-init 0"

ts_test "rate" "-src $mp4file -rate 10000 -pcr-init 0"

ts_test "singleau" "-src $mp4file -single-au -pcr-init 0"

ts_test "multiau" "-src $mp4file -multi-au -pcr-init 0"

ts_test "temi" "-src $mp4file -temi -pcr-init 0"

ts_test "pcr" "-src $mp4file -pcr-ms 40 -force-pcr-only -pcr-init 0 -pcr-offset 30000 -rap"

rm $mp4file
