#!/bin/sh


dash_cue_test()
{

test_begin $1

do_test "$MP4BOX -dash 1000 -profile live -out $TEMP_DIR/file.mpd $mp4file -cues $2 -strict-cues" "dash"

do_hash_test $TEMP_DIR/file_dash1.m4s "$1-hash-1st-seg"

test_end

}



dash_cue_test_file()
{

for cue in $MEDIA_DIR/dash_cues/*.xml ; do

name=$(basename $cue)
name=${name%%.*}

dash_cue_test "$name-$1" $cue

done

}

#this set of test has one error when using counter_cues_cts_broken.xml
mp4file=$TEMP_DIR/file.mp4
$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_openGOP_640x360_160kbps.264 -new $mp4file 2> 0

dash_cue_test_file "edits"

#this set of test has one error when using counter_cues_cts_broken.xml and one when using counter_cues_cts.xml because we have negctts
#so cts no longer match
$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_openGOP_640x360_160kbps.264:negctts -new $mp4file 2> 0

dash_cue_test_file "negctts"

rm $mp4file


