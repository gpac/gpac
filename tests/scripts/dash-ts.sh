#!/bin/sh

test_begin "dash-ts"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264 -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.aac -new $TEMP_DIR/file.mp4" "ts-for-dash-input-preparation"

#force a PCR init to avoid random PCR init value
do_test "$GPAC -i $TEMP_DIR/file.mp4 -o $TEMP_DIR/file.ts:pcr_init=10000:pes_pack=none" "ts-for-dash-input"

do_test "$MP4BOX -dash 1000 -rap -single-file -segment-name myrep/ts-segment-single-f-\$RepresentationID\$ $TEMP_DIR/file.ts -out $TEMP_DIR/file1.mpd" "ts-dash-single-file"
do_hash_test $TEMP_DIR/file1.mpd "mpd-single"

myinspect=$TEMP_DIR/inspect_single.txt
do_test "$GPAC -i $TEMP_DIR/file1.mpd inspect:all:deep:interleave=false:log=$myinspect"
do_hash_test $myinspect "inspect-single"

do_test "$MP4BOX -dash 1000 -rap -segment-name myrep/ts-segment-multiple-f-\$RepresentationID\$ $TEMP_DIR/file.ts -out $TEMP_DIR/file2.mpd" "ts-dash-multiple-file"
do_hash_test $TEMP_DIR/file2.mpd "mpd-multi"

myinspect=$TEMP_DIR/inspect_multi.txt
do_test "$GPAC -i $TEMP_DIR/file2.mpd inspect:all:deep:interleave=false:log=$myinspect"
do_hash_test $myinspect "inspect-multi"

test_end
