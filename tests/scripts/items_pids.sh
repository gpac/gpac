#!/bin/sh

if [ $EXTERNAL_MEDIA_AVAILABLE = 0 ] ; then
  return
fi

COUNTERFILE=$EXTERNAL_MEDIA_DIR/counter/counter_1280_720_I_25_tiled_500kb.hevc

test_begin "items-pids"

 if [ $test_skip != 1 ] ; then

iff_file="$TEMP_DIR/images.heic"
do_test "$MP4BOX -add-image $COUNTERFILE:dur=10:time=0 -add-image $COUNTERFILE:dur=10:time=1 -add-image $COUNTERFILE:dur=10:time=2 -add-image $COUNTERFILE:dur=10:time=3 -ab heic -new $iff_file" "create-iff"
do_hash_test $iff_file "create-iff"

myinspect=$TEMP_DIR/inspect.txt
do_test "$GPAC -i $iff_file inspect:log=$myinspect" "single-item"
do_hash_test $myinspect "single-item"

myinspect=$TEMP_DIR/inspect2.txt
do_test "$GPAC -i $iff_file:itt inspect:log=$myinspect" "single-pid"
do_hash_test $myinspect "single-pid"


#test item creation using N pids
dstfile=$TEMP_DIR/items-multi.heic
do_test "$GPAC -i $iff_file reframer @ -o $dstfile" "muxitem-multipid"
do_hash_test $myinspect "muxitem-multipid"

#test item creation using 1 pid
dstfile=$TEMP_DIR/items-single.heic
do_test "$GPAC -i $iff_file:itt reframer @ -o $dstfile" "muxitem-singlepid"
do_hash_test $myinspect "muxitem-singlepid"

#test item creation from raw hevc, filtering only SAP1
dstfile=$TEMP_DIR/items-raw.heic
do_test "$GPAC -i $COUNTERFILE:#ItemID=1 reframer:saps=1 @ -o $dstfile" "muxitem-singlepid"
do_hash_test $myinspect "muxitem-singlepid"


 fi

test_end

