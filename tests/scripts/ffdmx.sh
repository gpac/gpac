
ffdmx_test ()
{

test_begin "ffdmx-$1"

if [ $test_skip  = 1 ] ; then
return
fi

#ffmpeg behaves differently on various platforme (FPS detection, duration & ca might slightly differ).
#we therefore don't inspect the result but create an MP4 from it
#myinspect=$TEMP_DIR/inspect.txt
#do_test "$GPAC -no-reassign=no -i $2 inspect:allp:deep:interleave=false:log=$myinspect -graph -stats" "inspect"
#do_hash_test $myinspect "inspect"

dstfile=$TEMP_DIR/dump.mp4
do_test "$GPAC -no-reassign=no -i $2 -o $dstfile -graph -stats" "dump"
do_hash_test $dstfile "dump"

test_end
}

ffavin_test ()
{

test_begin "ffavin-$1"

if [ $test_skip  = 1 ] ; then
return
fi

myinspect=$TEMP_DIR/inspect.txt
do_test "$GPAC -no-reassign=no -i $2 inspect:dur=1/50:interleave=false:log=$myinspect -graph -stats" "inspect"
#no hash tests, live source and platform dependent

test_end
}


ffdmx_test "mkv" "$EXTERNAL_MEDIA_DIR/import/10bitwhite.mkv"
#test ffdmx on aac by forcing gfreg for input
ffdmx_test "aac" "$MEDIA_DIR/auxiliary_files/enst_audio.aac:gfreg=ffdmx"



config_linux=`gpac -h bin 2>&1 | grep GPAC_CONFIG_LINUX`
config_osx=`gpac -h bin 2>&1 | grep GPAC_CONFIG_DARWIN`
config_win=`gpac -h bin 2>&1 | grep GPAC_CONFIG_WIN32`

if [ -n "$config_osx" ] ; then
ffavin_test "screencap" "video://:gfreg=ffavin:fmt=avfoundation:dev=screen0:probes=0"
ffavin_test "screencap-all" "video://:gfreg=ffavin:fmt=avfoundation:dev=screen0:copy:sclock"
fi

if [ -n "$config_linux" ] ; then
ffavin_test "screencap" "video://:gfreg=ffavin:fmt=x11grab:dev=:0.0:probes=0"
ffavin_test "screencap-all" "video://:gfreg=ffavin:fmt=x11grab:dev=:0.0:copy:sclock"
fi

#todo
#if [ -z "$config_win" ] ; then
#ffavin_test "screencap" "video://:gfreg=ffavin:fmt=dshow:dev=screen-capture-recorder:probes=0"
#ffavin_test "screencap-all" "video://:gfreg=ffavin:fmt=dshow:dev=screen-capture-recorder:copy:sclock"
#fi
