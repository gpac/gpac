#!/bin/sh
inspectfilter="inspect:all:deep:interleave=false:test=noprop:fmt=%dts%-%cts%-%sap%%lf%"
dur=10
start=0

source=http://download.tsi.telecom-paristech.fr/gpac/DASH_CONFORMANCE/TelecomParisTech/mp4-live-1s/mp4-live-1s-mpd-AV-BS.mpd

dash_test ()
{

test_begin "dash-read-$1"

if [ $test_skip = 1 ] ; then
return
fi

myinspect=$TEMP_DIR/inspect.txt
do_test "$GPAC -i $2 $inspectfilter:dur=$dur:start=$start:log=$myinspect" "read"

if [ $3 != 0 ] ; then
do_hash_test $myinspect "read"
fi

test_end
}


dash_test "no-adapt" "$source:gpac:algo=none:start_with=max_bw" 1

algos="gbuf grate bba0 bolaf bolab bolau bolao"
for algo in $algos ; do
dash_test "$algo" "$source --algo=$algo"  1
done

dash_test "abort" "$source:gpac:abort"  1
dash_test "auto" "$source:gpac:auto_switch=1"  1
dash_test "bmin" "$source:gpac:use_bmin"  1
dash_test "xlink" "http://dash.akamaized.net/dash264/TestCases/5b/nomor/6.mpd" 1

start=255
dash_test "seek" "http://dash.akamaized.net/dash264/TestCases/5b/nomor/6.mpd" 1
start=100
dash_test "seek2" "http://dash.akamaized.net/dash264/TestCases/5b/nomor/6.mpd" 1
start=0

dash_test "ondemand" "http://download.tsi.telecom-paristech.fr/gpac/DASH_CONFORMANCE/TelecomParisTech/mp4-onDemand/mp4-onDemand-mpd-V.mpd" 1

dur=4
dash_test "live" "https://livesim.dashif.org/livesim/mup_30/testpic_2s/Manifest.mpd" 0
dash_test "live-timeline" "http://vm2.dashif.org/livesim-dev/segtimeline_1/testpic_6s/Manifest.mpd" 0

dash_test "hls" "https://devstreaming-cdn.apple.com/videos/streaming/examples/img_bipbop_adv_example_fmp4/master.m3u8 -broken-cert" 1

dash_test "smooth" "http://amssamples.streaming.mediaservices.windows.net/69fbaeba-8e92-4740-aedc-ce09ae945073/AzurePromo.ism/manifest" 1
