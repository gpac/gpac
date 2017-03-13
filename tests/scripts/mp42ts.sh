#test MP4Box RTP streaming and MP4 client

#DST="-dst=239.255.0.1"
DST=""

IFCE="127.0.0.1"

tsfile="$TEMP_DIR/test.ts"


ts_test ()
{

test_begin "mp42ts-$1"
if [ $test_skip  = 1 ] ; then
return
fi

do_test "$MP42TS $2" "file"
do_hash_test $tsfile "file"

do_playback_test "$tsfile" "play"

rm $tsfile
test_end
}

mp4file="$TEMP_DIR/test.mp4"

$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264 -add $MEDIA_DIR/auxiliary_files/enst_audio.aac -new $mp4file 2> /dev/null


ts_test "simple" "-src $mp4file -dst-file=$tsfile -pcr-init 0"

ts_test "rate" "-src $mp4file -dst-file=$tsfile -rate 10000 -pcr-init 0"

ts_test "singleau" "-src $mp4file -dst-file=$tsfile -single-au -pcr-init 0"

ts_test "multiau" "-src $mp4file -dst-file=$tsfile -multi-au -pcr-init 0"

ts_test "temi" "-src $mp4file -dst-file=$tsfile -temi -pcr-init 0"

ts_test "pcr" "-src $mp4file -dst-file=$tsfile -pcr-ms 40 -force-pcr-only -pcr-init 0 -pcr-offset 30000 -rap"

rm $mp4file
