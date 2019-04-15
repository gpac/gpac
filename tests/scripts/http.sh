#!/bin/sh

test_http()
{

test_begin "http-$1"

if [ $test_skip = 1 ] ; then
 return
fi

myinspect=$TEMP_DIR/inspect.txt
do_test "$GPAC -i $2 inspect:all:deep:test=network:interleave=false:log=$myinspect$3 -graph -stats"
do_hash_test $myinspect "inspect"

test_end

}

test_http "mp4-simple" "http://download.tsi.telecom-paristech.fr/gpac/gpac_test_suite/mp4/counter_video_360.mp4" ""

test_http "mp4-seek" "http://download.tsi.telecom-paristech.fr/gpac/gpac_test_suite/mp4/counter_video_360.mp4" ":dur=2.0:start=10"

test_http "aac-simple" "http://download.tsi.telecom-paristech.fr/gpac/gpac_test_suite/regression_tests/auxiliary_files/enst_audio.aac" ""

#on linux 32 bit we for now disable the aac seek, since the rounding of (start) and indexes in file gives a slightly different cts
do_aac_seek=1
config=`gpac -h bin 2>&1 | grep GPAC_HAS_64`
if [ -z $config ] ; then
config=`gpac -h bin 2>&1 | grep GPAC_CONFIG_LINUX`

if [ -n "$config" ] ; then
do_aac_seek=0
fi
fi

if [ $do_aac_seek != 0 ] ; then
test_http "aac-seek" "http://download.tsi.telecom-paristech.fr/gpac/gpac_test_suite/regression_tests/auxiliary_files/enst_audio.aac" ":dur=2.0:start=2"
fi

