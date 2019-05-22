#!/bin/sh

test_begin "dash-read"

 if [ $test_skip != 1 ] ; then


inspectfilter="inspect:all:deep:dur=10:interleave=false:test=noprop:fmt=%dts%-%cts%-%sap%%lf%"

source=http://download.tsi.telecom-paristech.fr/gpac/DASH_CONFORMANCE/TelecomParisTech/mp4-live-1s/mp4-live-1s-mpd-AV-BS.mpd
myinspect=$TEMP_DIR/inspect-none.txt
do_test "$GPAC -i $source:gpac:algo=none:start_with=max_bw $inspectfilter:log=$myinspect" "no-adapt"
do_hash_test $myinspect "no-adapt"

myinspect=$TEMP_DIR/inspect-adapt.txt
do_test "$GPAC -i $source $inspectfilter:log=$myinspect" "adapt"
do_hash_test $myinspect "adapt"

myinspect=$TEMP_DIR/inspect-adapt.txt
do_test "$GPAC -i $source:gpac:abort $inspectfilter::log=$myinspect" "adapt-abort"
do_hash_test $myinspect "adapt-abort"

algos="grate bba0 bolaf bolab bolau bolau"

for algo in $algos ; do
myinspect=$TEMP_DIR/inspect-$algo.txt
do_test "$GPAC -i $source --algo=$algo $inspectfilter:log=$myinspect" "$algo"
do_hash_test $myinspect "$algo"
done


myinspect=$TEMP_DIR/inspect-xlink.txt
do_test "$GPAC -i http://dash.akamaized.net/dash264/TestCases/5b/nomor/6.mpd $inspectfilter:log=$myinspect" "xlink"
do_hash_test $myinspect "xlink"

fi
test_end


test_begin "dash-read-live"
if [ $test_skip != 1 ] ; then

inspectfilter="inspect:dur=2:interleave=false"

myinspect=$TEMP_DIR/inspect-live.txt
do_test "$GPAC -i https://livesim.dashif.org/livesim/mup_30/testpic_2s/Manifest.mpd $inspectfilter:log=$myinspect" "live"
#cannot hash test this is a live source

fi
test_end
