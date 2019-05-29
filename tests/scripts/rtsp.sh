#test RTP streaming and MP4 client

IP="127.0.0.1"
MCASTIP="234.0.0.1"
IFCE="127.0.0.1"

rtsp_test_single ()
{

test_begin "rtsp-single-$1"
if [ $test_skip  = 1 ] ; then
return
fi

#start rtsp server
do_test "$GPAC -i $2 -o rtsp://$IP/testsession $3 -stats" "server-single" &

sleep 1

myinspect=$TEMP_DIR/inspect.txt

do_test "$GPAC -i rtsp://$IP/testsession inspect:deep:all:dur=1/1:log=$myinspect $4 -stats -graph" "dump"

wait

do_hash_test $myinspect "dump-inspect"

test_end
}


rtsp_test_server ()
{

test_begin "rtsp-server-$1"
if [ $test_skip  = 1 ] ; then
return
fi

#start rtsp server
do_test "$GPAC -runfor=4000 rtspout:runfor=1500:mounts=$MEDIA_DIR/auxiliary_files/ $3 -stats" "server" &

sleep 1

myinspect=$TEMP_DIR/inspect.txt

do_test "$GPAC -i $2 inspect:deep:all:dur=1/1:interleave=false:log=$myinspect $4 -stats -graph" "dump"

wait

do_hash_test $myinspect "dump-inspect"

test_end
}


rtsp_test_single "regular" $MEDIA_DIR/auxiliary_files/enst_audio.aac " --tso=10000" ""
rtsp_test_single "interleave" $MEDIA_DIR/auxiliary_files/enst_audio.aac " --tso=10000" " --interleave"

rtsp_test_server "regular" "rtsp://localhost/enst_audio.aac" " --tso=10000" " "
rtsp_test_server "dynurl" 'rtsp://localhost/@enst_audio.aac@enst_video.h264' " --tso=10000 --dynurl" ""

rtsp_test_server "mcast" "rtsp://localhost/enst_audio.aac" " --tso=10000 --mcast=on" " --force_mcast=$MCASTIP "
