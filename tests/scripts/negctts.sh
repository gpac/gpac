#!/bin/sh
test_begin "import-File-with-negative-CTS-DTS-offsets"

 if [ $test_skip  = 1 ] ; then
  return
 fi


do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/3D-HEVC/stream_bbb.bit:fmt=HEVC:negctts -new $TEMP_DIR/test.mp4" "mp4box-import-negctts"

do_hash_test "$TEMP_DIR/test.mp4" "import"

do_test "$MP4BOX -dtsc $TEMP_DIR/test.mp4" "mp4box-dump-negctts"
do_hash_test "$TEMP_DIR/test_ts.txt" "dump"

do_test "$GPAC -i $TEMP_DIR/test.mp4 -o $TEMP_DIR/test.ts:pcr_init=1000000:pes_pack=none" "tsmux"
do_hash_test "$TEMP_DIR/test.ts" "m2tsmux"

#for coverage, test m2ts dump with mp4box
do_test "$MP4BOX -logs container@debug -dm2ts $TEMP_DIR/test.ts" "dump_m2ts"
do_hash_test $TEMP_DIR/test.ts_prog_1_timestamps.txt "dump_m2ts"

do_test "$GPAC -i $TEMP_DIR/test.ts:dsmcc inspect" "inspect"

test_end
