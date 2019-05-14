#!/bin/sh

test_begin "dash-read"

 if [ $test_skip  = 1 ] ; then
  return
 fi

source=http://download.tsi.telecom-paristech.fr/gpac/DASH_CONFORMANCE/TelecomParisTech/mp4-live-1s/mp4-live-1s-mpd-AV-BS.mpd
myinspect=$TEMP_DIR/inspect.txt
do_test "$GPAC -i $source:gpac:algo=none:start_with=max_bw inspect:all:deep:dur=10:interleave=false:log=$myinspect" "no-adapt"
do_hash_test $myinspect "no-adapt"

myinspect=$TEMP_DIR/inspect2.txt
do_test "$GPAC -i $source inspect:all:deep:dur=10:interleave=false:log=$myinspect:fmt=%dts%-%cts%-%sap%%lf%" "adapt"
do_hash_test $myinspect "adapt"

test_end
