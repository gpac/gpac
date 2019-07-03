#test RTP streaming and MP4 client

DST="127.0.0.1"
IFCE="127.0.0.1"

rtp_test ()
{

test_begin "rtp-$1"
if [ $test_skip  = 1 ] ; then
return
fi

param=$3
inspect_only=0
if [ "$3" = "inspect_only" ] ; then
inspect_only=1
param=""
fi

#prepare the sdp
do_test "$GPAC -i $2 -o $TEMP_DIR/session.sdp:runfor=0:ip=$DST$param" "init"
do_hash_test "$TEMP_DIR/session.sdp" "init"

if [ $inspect_only = 1 ] ; then

myinspect=$TEMP_DIR/inspect.txt

do_test "$GPAC -for-test -runfor=2500 -i $TEMP_DIR/session.sdp:ifce=$IFCE inspect:deep:allp:log=$myinspect -stats -graph" "dump" &

sleep 1
#run without loop and tso=100000 to avoid a rand() that might impact TS rounding differently (hence slightly different durations->different hashes)
do_test "$GPAC -i $2 -o $TEMP_DIR/session.sdp:loop=no:ip=$DST:ifce=$IFCE:tso=100000 -stats" "stream"

wait

do_hash_test $myinspect "dump-inspect"

else

#a bit of fun: we export to native, isobmf, ts and DASH at the same time
#for TS dump, we need no pcr offset (or at least no random value init, so use 0), and single AU per pes otherwise we may have different execution results depending on how fast packets are received
dump_native=$TEMP_DIR/dump.$1
dump_mp4=$TEMP_DIR/dump.mp4
dump_ts=$TEMP_DIR/dump.ts
dump_mpd=$TEMP_DIR/dump.mpd
do_test "$GPAC -for-test -runfor=2000 -i $TEMP_DIR/session.sdp:ifce=$IFCE$4 -o $dump_native -o $dump_mp4 -o $dump_ts:pcr_init=0:pes_pack=none -o $dump_mpd -stats -graph" "dump" &

sleep .1
#run without loop and tso=100000 to avoid a rand() that might impact TS rounding differently (hence slightly different durations->different hashes)
do_test "$GPAC -i $2 -o $TEMP_DIR/session.sdp:loop=no:ip=$DST:ifce=$IFCE:tso=100000$param -stats" "stream"

wait

do_hash_test $dump_native "dump-native"
do_hash_test $dump_mp4 "dump-mp4"
do_hash_test $dump_ts "dump-ts"
do_hash_test $dump_mpd "dump-dash-mpd"
do_hash_test $TEMP_DIR/session_dashinit.mp4 "dump-dash-init"
do_hash_test $TEMP_DIR/session_dash1.m4s "dump-dash-seg"

fi

test_end
}

MYTMP=$TEMP_DIR

mp4file="$MYTMP/aac.mp4"
$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_audio.aac:dur=1 -new $mp4file 2> /dev/null
rtp_test "aac" $mp4file "" ""
rtp_test "latm" $mp4file ":latm" ":nat_keepalive=500"
#rm $mp4file > /dev/null

$MP4BOX -crypt $MEDIA_DIR/encryption/isma.xml $mp4file 2> /dev/null
rtp_test "aac-crypted" $mp4file "inspect_only" ""

mp4file="$MYTMP/mp3.mp4"
$MP4BOX -add $MEDIA_DIR/auxiliary_files/count_english.mp3:dur=1 -new $mp4file 2> /dev/null
rtp_test "mp3" $mp4file "" ""
#rm $mp4file > /dev/null

mp4file="$MYTMP/avc.mp4"
$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264:dur=1 -new $mp4file 2> /dev/null
rtp_test "avc" $mp4file ":mtu=500" ""

single_test "$GPAC -i $mp4file -o $TEMP_DIR/session.sdp:loop=no:ip=$DST:ifce=$IFCE:xps" "avc-xps"

#rm $mp4file > /dev/null

mp4file="$MYTMP/cmp.mp4"
$MP4BOX -add $MEDIA_DIR/auxiliary_files/count_video.cmp:dur=1 -new $mp4file 2> /dev/null
rtp_test "cmp" $mp4file "" ""
#rm $mp4file > /dev/null

mp4file="$MYTMP/ttxt.mp4"
$MP4BOX -add $MEDIA_DIR/auxiliary_files/subtitle.srt:dur=2 -new $mp4file 2> /dev/null
rtp_test "srt" $mp4file "inspect_only" ""
#rm $mp4file > /dev/null

mp4file="$MYTMP/hevc.mp4"
$MP4BOX -add $MEDIA_DIR/auxiliary_files/counter.hvc:dur=1 -new $mp4file 2> /dev/null
rtp_test "hvc" $mp4file "" ""
#rm $mp4file > /dev/null


if [ $EXTERNAL_MEDIA_AVAILABLE = 0 ] ; then
return
fi

mp4file="$MYTMP/h263.mp4"
$MP4BOX -add $EXTERNAL_MEDIA_DIR/import/bear_video.263:dur=1 -new $mp4file 2> /dev/null
rtp_test "h263" $mp4file "" ""
#rm $mp4file > /dev/null

mp4file="$MYTMP/amr.mp4"
$MP4BOX -add $EXTERNAL_MEDIA_DIR/import/bear_audio.amr:dur=1 -new $mp4file 2> /dev/null
rtp_test "amr" $mp4file "" ""
#rm $mp4file > /dev/null

mp4file="$MYTMP/mpeg1.mp4"
$MP4BOX -add $EXTERNAL_MEDIA_DIR/import/dead.m1v:dur=1 -new $mp4file 2> /dev/null
rtp_test "m1v" $mp4file "" ""
#rm $mp4file > /dev/null


mp4file="$MYTMP/ac3.mp4"
$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.ac3:dur=1 -new $mp4file 2> /dev/null
rtp_test "ac3" $mp4file "" ""
#rm $mp4file > /dev/null

mp4file="$MYTMP/qcelp.mp4"
$MP4BOX -add $EXTERNAL_MEDIA_DIR/import/counter_english.qcp:dur=1 -new $mp4file 2> /dev/null
rtp_test "qcp" $mp4file "inspect_only" ""
#rm $mp4file > /dev/null

